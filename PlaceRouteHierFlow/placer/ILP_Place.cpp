#include "ILP_solver.h"

#include <stdexcept>

#include "spdlog/spdlog.h"
#include "CbcModel.hpp"
#include "OsiClpSolverInterface.hpp"
#define BOOST_ALLOW_DEPRECATED_HEADERS
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>

std::vector<std::set<int>> ILP_solver::GetCC(const design& mydesign) const
{
  using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS>;
  Graph graph;
  for (const auto& it : mydesign.Ordering_Constraints) {
    add_edge(it.first.first, it.first.second, graph);
  }

  std::map<Graph::vertex_descriptor, int> compmap;
  int num = connected_components(graph, boost::make_assoc_property_map(compmap));
  std::map<int, std::set<int>> cc;
  for (auto& it : compmap) {
    cc[it.second].insert(it.first);
  }

  std::vector<std::set<int>> ret;
  for (auto& it : cc) {
    ret.push_back(std::move(it.second));
  }
  return ret;
}

double ILP_solver::PlaceUsingILP(const design& mydesign, const SeqPair& curr_sp, const PnRDB::Drc_info& drcInfo, const int num_threads) {

  if (mydesign.Blocks.empty()) return -1;
  auto logger = spdlog::default_logger()->clone("placer.ILP_solver.PlaceUsingILP");
  ++const_cast<design&>(mydesign)._totalNumCostCalc;
  if (mydesign.Blocks.size() == 1) {
    Blocks[0].x = 0; Blocks[0].y = 0;
    Blocks[0].H_flip = 0; Blocks[0].V_flip = 0;
    area_ilp = ((double)mydesign.Blocks[0][curr_sp.selected[0]].width) * ((double)mydesign.Blocks[0][curr_sp.selected[0]].height);
  } else {
    if (mydesign.leftAlign()) {
      // frame and solve ILP to flush bottom/left
      if (!PlaceILPSymphony_select(mydesign, curr_sp, drcInfo, num_threads, true))  return -1;
    } else if (mydesign.rightAlign()) {
      if (!PlaceILPSymphony_select(mydesign, curr_sp, drcInfo, num_threads, false)) return -1;
    } else {
      if (!PlaceILPSymphony_select(mydesign, curr_sp, drcInfo, num_threads, true))  return -1;
      std::vector<Block> blockslocal{Blocks};
      auto selectedlocal = curr_sp.selected;
      // frame and solve ILP to flush top/right
      if (!PlaceILPSymphony_select(mydesign, curr_sp, drcInfo, num_threads, false) 
          || !MoveBlocksUsingSlack(blockslocal, mydesign, curr_sp, drcInfo, num_threads, false)) {
        // if unable to solve flush top/right or if the solution changed significantly,
        // use the bottom/left flush solution
        Blocks = blockslocal;
        const_cast<SeqPair&>(curr_sp).selected = selectedlocal;
      }
    }
    // snap up coordinates to grid
    for (unsigned i = 0; i < mydesign.Blocks.size(); i++) {
      roundup(Blocks[i].x, x_pitch);
      roundup(Blocks[i].y, y_pitch);
    }
  }

  TimeMeasure tm(const_cast<design&>(mydesign).gen_valid_runtime);
  // calculate LL and UR
  LL.x = INT_MAX, LL.y = INT_MAX;
  UR.x = INT_MIN, UR.y = INT_MIN;
  for (int i = 0; i < mydesign.Blocks.size(); i++) {
    LL.x = std::min(LL.x, Blocks[i].x);
    LL.y = std::min(LL.y, Blocks[i].y);
    UR.x = std::max(UR.x, Blocks[i].x + mydesign.Blocks[i][curr_sp.selected[i]].width);
    UR.y = std::max(UR.y, Blocks[i].y + mydesign.Blocks[i][curr_sp.selected[i]].height);
  }
  // calculate area
  area = double(UR.x - LL.x) * double(UR.y - LL.y);
  // calculate norm area
  area_norm = area * 0.1 / mydesign.GetMaxBlockAreaSum();
  // calculate ratio
  // ratio = std::max(double(UR.x - LL.x) / double(UR.y - LL.y), double(UR.y - LL.y) / double(UR.x - LL.x));
  ratio = double(UR.x - LL.x) / double(UR.y - LL.y);
  if (ratio < Aspect_Ratio[0] || ratio > Aspect_Ratio[1]) {
    ++const_cast<design&>(mydesign)._infeasAspRatio;
    return -1;
  }
  if (placement_box[0] > 0 && (UR.x - LL.x > placement_box[0]) || placement_box[1] > 0 && (UR.y - LL.y > placement_box[1])) {
    ++const_cast<design&>(mydesign)._infeasPlBound;
    return -1;
  }
  // calculate HPWL
  HPWL = 0;
  HPWL_extend = 0;
  HPWL_extend_terminal = 0;
  for (const auto& neti : mydesign.Nets) {
    int HPWL_min_x = UR.x, HPWL_min_y = UR.y, HPWL_max_x = 0, HPWL_max_y = 0;
    int HPWL_extend_min_x = UR.x, HPWL_extend_min_y = UR.y, HPWL_extend_max_x = 0, HPWL_extend_max_y = 0;
    for (const auto& connectedj : neti.connected) {
      if (connectedj.type == placerDB::Block) {
        int iter2 = connectedj.iter2, iter = connectedj.iter;
        for (const auto& centerk : mydesign.Blocks[iter2][curr_sp.selected[iter2]].blockPins[iter].center) {
          // calculate contact center
          int pin_x = centerk.x;
          int pin_y = centerk.y;
          if (Blocks[iter2].H_flip) pin_x = mydesign.Blocks[iter2][curr_sp.selected[iter2]].width - pin_x;
          if (Blocks[iter2].V_flip) pin_y = mydesign.Blocks[iter2][curr_sp.selected[iter2]].height - pin_y;
          pin_x += Blocks[iter2].x;
          pin_y += Blocks[iter2].y;
          HPWL_min_x = std::min(HPWL_min_x, pin_x);
          HPWL_max_x = std::max(HPWL_max_x, pin_x);
          HPWL_min_y = std::min(HPWL_min_y, pin_y);
          HPWL_max_y = std::max(HPWL_max_y, pin_y);
        }
        for (const auto& boundaryk : mydesign.Blocks[iter2][curr_sp.selected[iter2]].blockPins[iter].boundary) {
          int pin_llx = boundaryk.polygon[0].x, pin_urx = boundaryk.polygon[2].x;
          int pin_lly = boundaryk.polygon[0].y, pin_ury = boundaryk.polygon[2].y;
          if (Blocks[iter2].H_flip) {
            pin_llx = mydesign.Blocks[iter2][curr_sp.selected[iter2]].width - boundaryk.polygon[2].x;
            pin_urx = mydesign.Blocks[iter2][curr_sp.selected[iter2]].width - boundaryk.polygon[0].x;
          }
          if (Blocks[iter2].V_flip) {
            pin_lly = mydesign.Blocks[iter2][curr_sp.selected[iter2]].height - boundaryk.polygon[2].y;
            pin_ury = mydesign.Blocks[iter2][curr_sp.selected[iter2]].height - boundaryk.polygon[0].y;
          }
          pin_llx += Blocks[iter2].x;
          pin_urx += Blocks[iter2].x;
          pin_lly += Blocks[iter2].y;
          pin_ury += Blocks[iter2].y;
          HPWL_extend_min_x = std::min(HPWL_extend_min_x, pin_llx);
          HPWL_extend_max_x = std::max(HPWL_extend_max_x, pin_urx);
          HPWL_extend_min_y = std::min(HPWL_extend_min_y, pin_lly);
          HPWL_extend_max_y = std::max(HPWL_extend_max_y, pin_ury);
        }
      }
    }
    HPWL += (HPWL_max_y - HPWL_min_y) + (HPWL_max_x - HPWL_min_x);
    HPWL_extend += (HPWL_extend_max_y - HPWL_extend_min_y) + (HPWL_extend_max_x - HPWL_extend_min_x);
    bool is_terminal_net = false;
    for (const auto& c : neti.connected) {
      if (c.type == placerDB::Terminal) {
        is_terminal_net = true;
        break;
      }
    }
    if (is_terminal_net) HPWL_extend_terminal += (HPWL_extend_max_y - HPWL_extend_min_y) + (HPWL_extend_max_x - HPWL_extend_min_x);
  }

  // HPWL norm
  if (!mydesign.Nets.empty()) HPWL_norm = HPWL_extend / mydesign.GetMaxBlockHPWLSum() / double(mydesign.Nets.size());
  // calculate linear constraint
  linear_const = 0;
  std::vector<std::vector<double>> feature_value;
  for (const auto& neti : mydesign.Nets) {
    std::vector<std::vector<placerDB::point>> center_points;
    for (const auto& connectedj : neti.connected) {
      if (connectedj.type == placerDB::Block) {
        std::vector<placerDB::point> pos;
        for (const auto& ci : mydesign.Blocks[connectedj.iter2][curr_sp.selected[connectedj.iter2]].blockPins[connectedj.iter].center) {
          placerDB::point newp;
          newp.x = ci.x;
          newp.y = ci.y;
          if (Blocks[connectedj.iter2].H_flip) newp.x = mydesign.Blocks[connectedj.iter2][curr_sp.selected[connectedj.iter2]].width - newp.x;
          if (Blocks[connectedj.iter2].V_flip) newp.y = mydesign.Blocks[connectedj.iter2][curr_sp.selected[connectedj.iter2]].height - newp.y;
          newp.x += Blocks[connectedj.iter2].x;
          newp.y += Blocks[connectedj.iter2].y;
          pos.push_back(newp);
        }
        center_points.push_back(pos);
      } else if (connectedj.type == placerDB::Terminal) {
        center_points.push_back({mydesign.Terminals[connectedj.iter].center});
      }
    }
    std::vector<double> temp_feature = Calculate_Center_Point_feature(center_points);
    feature_value.push_back(temp_feature);
    double temp_sum = 0;
    for (int j = 0; j < neti.connected.size(); j++) temp_sum += neti.connected[j].alpha * temp_feature[j];
    temp_sum = std::max(temp_sum - neti.upperBound, double(0));
    linear_const += temp_sum;
  }

  if (!mydesign.Nets.empty()) linear_const /= (mydesign.GetMaxBlockHPWLSum() * double(mydesign.Nets.size()));

  // calculate multi linear constraint
  multi_linear_const = 0;
  for (const auto& constrainti : mydesign.ML_Constraints) {
    double temp_sum = 0;
    for (const auto& constraintj : constrainti.Multi_linearConst) {
      for (int k = 0; k < constraintj.pins.size(); k++) {
        int index_i = 0;
        int index_j = 0;
        for (int m = 0; m < mydesign.Nets.size(); m++) {
          for (int n = 0; n < mydesign.Nets[m].connected.size(); n++) {
            if (mydesign.Nets[m].connected[n].iter2 == constraintj.pins[k].first && mydesign.Nets[m].connected[n].iter == constraintj.pins[k].second) {
              index_i = m;
              index_j = n;
              break;
            }
          }
        }
        temp_sum += constraintj.alpha[k] * feature_value[index_i][index_j];
      }
    }
    temp_sum = std::min(temp_sum, constrainti.upperBound);
    multi_linear_const += temp_sum;
  }

  double calculated_cost = CalculateCost(mydesign, curr_sp);
  cost = calculated_cost;
  if (cost >= 0.) {
    logger->debug("ILP__HPWL_compare block {0} : HPWL_extend={1} HPWL_ILP={2}", mydesign.name, HPWL_extend, HPWL_ILP);
    logger->debug("ILP__Area_compare block {0} : area={1} area_ilp={2}", mydesign.name, area, area_ilp);
  }
  return calculated_cost;
}

bool ILP_solver::PlaceILPSymphony_select(const design& mydesign, const SeqPair& curr_sp, const PnRDB::Drc_info& drcInfo, const int num_threads, bool flushbl, const vector<placerDB::point>* prev) {
  TimeMeasure tm(const_cast<design&>(mydesign).ilp_runtime);
  auto logger = spdlog::default_logger()->clone("placer.ILP_solver.PlaceILPSymphony_select");

  int v_metal_index = -1;
  int h_metal_index = -1;
  for (unsigned int i = 0; i < drcInfo.Metal_info.size(); ++i) {
    if (drcInfo.Metal_info[i].direct == 0) {
      v_metal_index = i;
      break;
    }
  }
  for (unsigned int i = 0; i < drcInfo.Metal_info.size(); ++i) {
    if (drcInfo.Metal_info[i].direct == 1) {
      h_metal_index = i;
      break;
    }
  }
  x_pitch = drcInfo.Metal_info[v_metal_index].grid_unit_x;
  y_pitch = drcInfo.Metal_info[h_metal_index].grid_unit_y;

  // each block has 4 vars, x, y, H_flip, V_flip;
  const unsigned N_block_vars_max = mydesign.Blocks.size() * 6;
  const unsigned N_net_vars_max   = N_block_vars_max + mydesign.Nets.size() * 4;
  const unsigned N_aspect_ratio_max = N_net_vars_max + 2;
  const unsigned N_area_max         = N_aspect_ratio_max + 2;
  unsigned N_var{N_area_max};

  unsigned count_blocks{0};
  for (const auto& blk : mydesign.Blocks) count_blocks += blk.size();

  std::map<std::pair<int, int>, std::tuple<int, int, int>> pin_idx_map;
  for (unsigned int i = 0; i < mydesign.Nets.size(); i++) {
    if (mydesign.Nets[i].connected.size() < 2) continue;
    int cnt{0};
    for (unsigned int j = 0; j < mydesign.Nets[i].connected.size(); j++) {
      if (mydesign.Nets[i].connected[j].type == placerDB::Block) {
        ++cnt;
      }
    }
    if (cnt <2) continue;
    for (unsigned int j = 0; j < mydesign.Nets[i].connected.size(); j++) {
      if (mydesign.Nets[i].connected[j].type == placerDB::Block) {
        const int block_id = mydesign.Nets[i].connected[j].iter2;
        const int pin_id = mydesign.Nets[i].connected[j].iter;
        auto idp = std::make_pair(block_id, pin_id);
        if (pin_idx_map.find(idp) == pin_idx_map.end()) {
          pin_idx_map.emplace(idp, std::make_tuple(N_var, INT_MIN, INT_MIN));
          if (mydesign.Blocks[block_id].size() > 1) N_var += 8;
          else N_var += 4;
        }
      }
    }
  }

  const unsigned N_var_max{N_var + count_blocks + (unsigned)(mydesign.Blocks.size() * (mydesign.Blocks.size() - 1))};
  // i*6:x
  // i*6+1:y
  // i*6+2:H_flip
  // i*6+3:V_flip
  // i*6+4:Width
  // i*6+5:Height
  OsiClpSolverInterface osiclp;
  const double infty{osiclp.getInfinity()};

  std::vector<int> rowindofcol[N_var_max];
  std::vector<double> constrvalues[N_var_max];
  std::vector<double> rhs;
  std::vector<char> intvars;
  intvars.reserve(N_var_max);
  intvars.resize(N_var, 0);
  std::vector<char> sens, rowtype;
  std::vector<double> collb, colub;
  collb.reserve(N_var_max); colub.reserve(N_var_max);
  collb.resize(N_var, 0);   colub.resize(N_var, infty);
  sens.reserve(curr_sp.posPair.size() * curr_sp.posPair.size() * 5);
  rowtype.reserve(curr_sp.posPair.size() * curr_sp.posPair.size() * 5);
  rhs.reserve(curr_sp.posPair.size() * curr_sp.posPair.size() * 5);


  //flip variables are binary
  for (unsigned i = 0; i < mydesign.Blocks.size(); i++) {
    colub[i * 6 + 2] = 1;
    colub[i * 6 + 3] = 1;
    intvars[i * 6 + 2] = 1;
    intvars[i * 6 + 3] = 1;
  }

  double aspectratio = (Aspect_Ratio[0] + Aspect_Ratio[1])/2.;
  if (aspectratio > 20) {
    aspectratio = 1.;
  }
  int maxhierwidth{0}, maxhierheight{0};
  for (unsigned i = 0; i < mydesign.Blocks.size(); ++i) {
    maxhierwidth += mydesign.Blocks[i][curr_sp.selected[i]].width;
    maxhierheight += mydesign.Blocks[i][curr_sp.selected[i]].height;
  }

  if (flushbl) {
    for (int i = 0; i < Blocks.size(); ++i) {
      int minwidth{INT_MAX}, minheight{INT_MAX};
      int maxwidth{0}, maxheight{0};
      for (auto& it : mydesign.Blocks[i]) {
        minwidth = std::min(minwidth, it.width);
        minheight = std::min(minheight, it.height);
        maxwidth = std::max(maxwidth, it.width);
        maxheight = std::max(maxheight, it.height);
      }
      collb[i * 6 + 4] = minwidth;  colub[i * 6 + 4] = maxwidth;
      collb[i * 6 + 5] = minheight; colub[i * 6 + 5] = maxheight;
      if (prev) {
        collb[i * 6]     = (*prev)[i].x;
        collb[i * 6 + 1] = (*prev)[i].y;
      } else {
        colub[i * 6]     = maxhierwidth;
        colub[i * 6 + 1] = maxhierheight;
      }
    }
    for (unsigned i = 0; i < mydesign.Nets.size(); ++i) {
      auto ind = N_block_vars_max + i * 4;
      collb[ind]     = 0; colub[ind]     = maxhierwidth; 
      collb[ind + 1] = 0; colub[ind + 1] = maxhierheight;
      collb[ind + 2] = 0; colub[ind + 2] = maxhierwidth; 
      collb[ind + 3] = 0; colub[ind + 3] = maxhierheight;
    }
    colub[N_area_max - 1] = maxhierheight;
    colub[N_area_max - 2] = maxhierwidth;
  } else {
    for (int i = 0; i < Blocks.size(); ++i) {
      int minwidth{INT_MAX}, minheight{INT_MAX};
      int maxwidth{0}, maxheight{0};
      for (auto& it : mydesign.Blocks[i]) {
        minwidth = std::min(minwidth, it.width);
        minheight = std::min(minheight, it.height);
        maxwidth = std::max(maxwidth, it.width);
        maxheight = std::max(maxheight, it.height);
      }
      collb[i * 6]     = -maxhierwidth;  colub[i * 6]     = -minwidth;
      collb[i * 6 + 1] = -maxhierheight; colub[i * 6 + 1] = -minheight;
      collb[i * 6 + 4] = minwidth;  colub[i * 6 + 4] = maxwidth;
      collb[i * 6 + 5] = minheight; colub[i * 6 + 5] = maxheight;
    }
    for (unsigned i = 0; i < mydesign.Nets.size(); ++i) {
      auto ind = N_block_vars_max + i * 4;
      collb[ind]     = -maxhierwidth;  colub[ind]     = 0;
      collb[ind + 1] = -maxhierheight; colub[ind + 1] = 0;
      collb[ind + 2] = -maxhierwidth;  colub[ind + 2] = 0;
      collb[ind + 3] = -maxhierheight; colub[ind + 3] = 0;
    }
    collb[N_area_max - 1] = -maxhierheight; colub[N_area_max - 1] = 0;
    collb[N_area_max - 2] = -maxhierwidth;  colub[N_area_max - 2] = 0;
  }
  colub[N_aspect_ratio_max - 1] = std::max(maxhierwidth, maxhierheight);
  colub[N_aspect_ratio_max - 2] = std::max(maxhierwidth, maxhierheight);

  ConstGraph const_graph;
  std::vector<double> objective(N_var_max, 0);
  for (unsigned int i = 0; i < mydesign.Nets.size(); i++) {
    if (mydesign.Nets[i].connected.size() < 2) continue;
    int cnt{0};
    for (unsigned int j = 0; j < mydesign.Nets[i].connected.size(); j++) {
      if (mydesign.Nets[i].connected[j].type == placerDB::Block) {
        ++cnt;
      }
    }
    if (cnt <2) continue;
    int ind = int(N_block_vars_max + i * 4);
    objective[ind]     = -const_graph.LAMBDA;
    objective[ind + 1] = -const_graph.LAMBDA;
    objective[ind + 2] = const_graph.LAMBDA;
    objective[ind + 3] = const_graph.LAMBDA;
  }
  if (flushbl) {
    objective[N_area_max - 1] = 1. * mydesign.Nets.size();
    objective[N_area_max - 2] = 1. * mydesign.Nets.size();
  } else {
    objective[N_area_max - 1] = -1. * mydesign.Nets.size();
    objective[N_area_max - 2] = -1. * mydesign.Nets.size();
  }
  objective[N_aspect_ratio_max - 1] = 0.1 * mydesign.Nets.size();
  objective[N_aspect_ratio_max - 2] = 0.1 * mydesign.Nets.size();

  int bias_Hgraph = mydesign.bias_Hgraph, bias_Vgraph = mydesign.bias_Vgraph;
  roundup(bias_Hgraph, x_pitch);
  roundup(bias_Vgraph, y_pitch);

  // constraint for |W - Aspect_ratio * H|
  if (flushbl) {
    rowindofcol[N_aspect_ratio_max - 1].push_back(rhs.size());
    rowindofcol[N_aspect_ratio_max - 2].push_back(rhs.size());
    rowindofcol[N_area_max - 2].push_back(rhs.size());
    rowindofcol[N_area_max - 1].push_back(rhs.size());
    constrvalues[N_aspect_ratio_max - 1].push_back(1);
    constrvalues[N_aspect_ratio_max - 2].push_back(-1);
    constrvalues[N_area_max - 2].push_back(-1);
    constrvalues[N_area_max - 1].push_back(aspectratio);
    sens.push_back('E');
    rhs.push_back(0.);
    rowtype.push_back('a');
  } else {
    rowindofcol[N_aspect_ratio_max - 1].push_back(rhs.size());
    rowindofcol[N_aspect_ratio_max - 2].push_back(rhs.size());
    rowindofcol[N_area_max - 2].push_back(rhs.size());
    rowindofcol[N_area_max - 1].push_back(rhs.size());
    constrvalues[N_aspect_ratio_max - 1].push_back(1);
    constrvalues[N_aspect_ratio_max - 2].push_back(-1);
    constrvalues[N_area_max - 2].push_back(1);
    constrvalues[N_area_max - 1].push_back(-aspectratio);
    sens.push_back('E');
    rhs.push_back(0.);
    rowtype.push_back('a');
  }

  const int maxxdim = maxhierwidth  * 5;
  const int maxydim = maxhierheight * 5;
  int maxdim = std::max(maxxdim, maxydim) * 2;

  const auto overlap_constr = GetCC(mydesign);
  std::map<int, const std::set<int>&> overlap_constr_map;
  for (const auto& it : overlap_constr) {
    for (auto& e : it) {
      overlap_constr_map.emplace(e, it);
    }
  }

  std::map<int, const std::set<int>&> align_constr_map_h, align_constr_map_v;
  std::vector<std::set<int>> align_constr_h, align_constr_v;
  for (unsigned i = 0; i < mydesign.Align_blocks.size(); ++i) {
    const auto& align = mydesign.Align_blocks[i];
    if (align.horizon) {
      align_constr_h.emplace_back(align.blocks.begin(), align.blocks.end());
      for (auto& it : align_constr_h.back()) {
        align_constr_map_h.emplace(it, align_constr_h.back());
      }
    } else {
      align_constr_v.emplace_back(align.blocks.begin(), align.blocks.end());
      for (auto& it : align_constr_v.back()) {
        align_constr_map_v.emplace(it, align_constr_v.back());
      }
    }
  }

  std::map<std::pair<int, int>, unsigned> buf_indx_map, buf_xy_indx_map;
  // overlap constraint
  unsigned buf_var_index{0};
  for (unsigned int i = 0; i < mydesign.Blocks.size(); i++) {
    auto itoverlap = overlap_constr_map.find(i);
    auto italignh  = align_constr_map_h.find(i);
    auto italignv  = align_constr_map_v.find(i);
    for (unsigned int j = i + 1; j < mydesign.Blocks.size(); j++) {
      if (itoverlap != overlap_constr_map.end() && itoverlap->second.find(j) != itoverlap->second.end()) continue;
      if (find(mydesign.Abut_Constraints.begin(), mydesign.Abut_Constraints.end(), make_pair(make_pair(int(i), int(j)), placerDB::H)) !=
              mydesign.Abut_Constraints.end()) continue;
      if (find(mydesign.Abut_Constraints.begin(), mydesign.Abut_Constraints.end(), make_pair(make_pair(int(i), int(j)), placerDB::V)) !=
              mydesign.Abut_Constraints.end()) continue;

      bool alignhij = (italignh != align_constr_map_h.end() && italignh->second.find(j) != italignh->second.end());
      bool alignvij = (italignv != align_constr_map_v.end() && italignv->second.find(j) != italignv->second.end());
      if (!alignhij && !alignvij) {
        buf_indx_map[std::make_pair(i, j)] = N_var++;
        buf_xy_indx_map[std::make_pair(i, j)] = N_var++;
        if (collb.size() < N_var) {
          collb.resize(N_var, 0);
          colub.resize(N_var, 1);
          intvars.resize(N_var, 1);
        }

        auto indxy = N_var - 1;
        auto ind   = N_var - 2;
        rowindofcol[i * 6].push_back(rhs.size());
        rowindofcol[j * 6].push_back(rhs.size());
        rowindofcol[ind].push_back(rhs.size());
        rowindofcol[indxy].push_back(rhs.size());
        rowindofcol[j * 6 + 4].push_back(rhs.size());
        constrvalues[i * 6].push_back(1);
        constrvalues[j * 6].push_back(-1);
        constrvalues[ind].push_back(maxxdim);
        constrvalues[indxy].push_back(maxdim);
        constrvalues[j * 6 + 4].push_back(-1);
        sens.push_back('G');
        rhs.push_back(0);
        rowtype.push_back('o');

        rowindofcol[i * 6].push_back(rhs.size());
        rowindofcol[j * 6].push_back(rhs.size());
        rowindofcol[ind].push_back(rhs.size());
        rowindofcol[indxy].push_back(rhs.size());
        rowindofcol[i * 6 + 4].push_back(rhs.size());
        constrvalues[i * 6].push_back(1);
        constrvalues[j * 6].push_back(-1);
        constrvalues[ind].push_back(maxxdim);
        constrvalues[indxy].push_back(-maxdim);
        constrvalues[i * 6 + 4].push_back(1);
        sens.push_back('L');
        rhs.push_back(maxxdim);
        rowtype.push_back('o');

        rowindofcol[i * 6 + 1].push_back(rhs.size());
        rowindofcol[j * 6 + 1].push_back(rhs.size());
        rowindofcol[ind].push_back(rhs.size());
        rowindofcol[indxy].push_back(rhs.size());
        rowindofcol[j * 6 + 5].push_back(rhs.size());
        constrvalues[i * 6 + 1].push_back(1);
        constrvalues[j * 6 + 1].push_back(-1);
        constrvalues[ind].push_back(maxydim);
        constrvalues[indxy].push_back(-maxdim);
        constrvalues[j * 6 + 5].push_back(-1);
        sens.push_back('G');
        rhs.push_back(-maxdim);
        rowtype.push_back('o');

        rowindofcol[i * 6 + 1].push_back(rhs.size());
        rowindofcol[j * 6 + 1].push_back(rhs.size());
        rowindofcol[ind].push_back(rhs.size());
        rowindofcol[indxy].push_back(rhs.size());
        rowindofcol[i * 6 + 5].push_back(rhs.size());
        constrvalues[i * 6 + 1].push_back(1);
        constrvalues[j * 6 + 1].push_back(-1);
        constrvalues[ind].push_back(maxydim);
        constrvalues[indxy].push_back(maxdim);
        constrvalues[i * 6 + 5].push_back(1);
        sens.push_back('L');
        rhs.push_back(maxydim + maxdim);
        rowtype.push_back('o');
      } else if (alignhij) {
        buf_indx_map[std::make_pair(i, j)] = N_var++;
        if (collb.size() < N_var) {
          collb.resize(N_var, 0);
          colub.resize(N_var, 1);
          intvars.resize(N_var, 1);
        }

        auto ind = N_var - 1;
        rowindofcol[i * 6].push_back(rhs.size());
        rowindofcol[j * 6].push_back(rhs.size());
        rowindofcol[ind].push_back(rhs.size());
        rowindofcol[j * 6 + 4].push_back(rhs.size());
        constrvalues[i * 6].push_back(1);
        constrvalues[j * 6].push_back(-1);
        constrvalues[ind].push_back(maxxdim);
        constrvalues[j * 6 + 4].push_back(-1);
        sens.push_back('G');
        rhs.push_back(0);
        rowtype.push_back('o');

        rowindofcol[i * 6].push_back(rhs.size());
        rowindofcol[j * 6].push_back(rhs.size());
        rowindofcol[ind].push_back(rhs.size());
        rowindofcol[i * 6 + 4].push_back(rhs.size());
        constrvalues[i * 6].push_back(1);
        constrvalues[j * 6].push_back(-1);
        constrvalues[ind].push_back(maxxdim);
        constrvalues[i * 6 + 4].push_back(1);
        sens.push_back('L');
        rhs.push_back(maxxdim);
        rowtype.push_back('o');
      } else if (alignvij) {
        buf_indx_map[std::make_pair(i, j)] = N_var++;
        if (collb.size() < N_var) {
          collb.resize(N_var, 0);
          colub.resize(N_var, 1);
          intvars.resize(N_var, 1);
        }

        auto ind = N_var - 1;
        rowindofcol[i * 6 + 1].push_back(rhs.size());
        rowindofcol[j * 6 + 1].push_back(rhs.size());
        rowindofcol[ind].push_back(rhs.size());
        rowindofcol[j * 6 + 5].push_back(rhs.size());
        constrvalues[i * 6 + 1].push_back(1);
        constrvalues[j * 6 + 1].push_back(-1);
        constrvalues[ind].push_back(maxydim);
        constrvalues[j * 6 + 5].push_back(-1);
        sens.push_back('G');
        rhs.push_back(0);
        rowtype.push_back('o');

        rowindofcol[i * 6 + 1].push_back(rhs.size());
        rowindofcol[j * 6 + 1].push_back(rhs.size());
        rowindofcol[ind].push_back(rhs.size());
        rowindofcol[i * 6 + 5].push_back(rhs.size());
        constrvalues[i * 6 + 1].push_back(1);
        constrvalues[j * 6 + 1].push_back(-1);
        constrvalues[ind].push_back(maxydim);
        constrvalues[i * 6 + 5].push_back(1);
        sens.push_back('L');
        rhs.push_back(maxydim);
        rowtype.push_back('o');
      }
    }
  }

  // \sigma (select_i,j) = 1 for all blocks i and variant j \in [0,N_i]
  // width_i = \sum select_i,j * width_i,j
  // height_i = \sum select_i,j * height_i,j
  unsigned blk_select_idx[mydesign.Blocks.size()] = {};
  for (unsigned i = 0; i < mydesign.Blocks.size(); ++i) {
    const auto& blk = mydesign.Blocks[i];
    blk_select_idx[i] = N_var;
    if (blk.size() > 1) {
      N_var += (unsigned)blk.size();
      if (collb.size() < N_var) {
        collb.resize(N_var, 0);
        colub.resize(N_var, 1);
        intvars.resize(N_var, 1);
      }
      for (unsigned j = N_var - blk.size(); j < N_var; ++j) {
        rowindofcol[j].push_back(rhs.size());
        constrvalues[j].push_back(1);
      }
      sens.push_back('E');
      rhs.push_back(1);
      rowtype.push_back('s');
      for (unsigned v : {0, 1}) {
        for (unsigned j = 0; j < blk.size(); ++j) {
          rowindofcol[N_var - blk.size() + j].push_back(rhs.size());
          constrvalues[N_var - blk.size() + j].push_back((v ? blk[j].height : blk[j].width));
        }
        rowindofcol[6 * i + 4 + v].push_back(rhs.size());
        constrvalues[6 * i + 4 + v].push_back(-1);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('s');
      }
    } else if (blk.size() == 1) {
      rowindofcol[6 * i + 4].push_back(rhs.size());
      constrvalues[6 * i + 4].push_back(1);
      sens.push_back('E');
      rowtype.push_back('s');
      rhs.push_back(blk[0].width);
      rowindofcol[6 * i + 5].push_back(rhs.size());
      constrvalues[6 * i + 5].push_back(1);
      sens.push_back('E');
      rhs.push_back(blk[0].height);
      rowtype.push_back('s');
    }
  }

  for (auto& it : pin_idx_map) {
    const int block_id = it.first.first;
    const int pin_id   = it.first.second;
    if (mydesign.Blocks[block_id].size() > 1) {
      std::vector<int> bbox_max(6, INT_MIN);
      std::vector<int> deltaxid, deltayid;
      bool deltaxneg{false}, deltayneg{false}, samedeltax{true}, samedeltay{true};
      int deltax{INT_MIN}, deltay{INT_MIN};
      for (unsigned i = 0; i < mydesign.Blocks[block_id].size(); ++i) {
        const auto& blk = mydesign.Blocks[block_id][i];
        int bbox[6];
        bbox[0] = blk.width / 2;  bbox[2] = blk.width / 2;
        bbox[1] = blk.height / 2; bbox[3] = blk.height / 2;
        if (blk.blockPins.size()) {
          bbox[0] = blk.blockPins[pin_id].bbox.LL.x;
          bbox[1] = blk.blockPins[pin_id].bbox.LL.y;
          bbox[2] = blk.blockPins[pin_id].bbox.UR.x;
          bbox[3] = blk.blockPins[pin_id].bbox.UR.y;
        }
        bbox[4] = blk.width  - bbox[0] - bbox[2];
        bbox[5] = blk.height - bbox[1] - bbox[3];
        if (deltax == INT_MIN) {
          deltax = bbox[4];
        } else if (deltax != bbox[4] && bbox[4] != 0 && deltax != 0) {
          samedeltax = false;
        }
        if (deltay == INT_MIN) {
          deltay = bbox[5];
        } else if (deltay != bbox[5] && bbox[5] != 0 && deltay != 0) {
          samedeltay = false;
        }

        if (bbox[4] < 0) deltaxneg = true;
        if (bbox[5] < 0) deltayneg = true;
        for (unsigned j = 0; j < 4; ++j) {
          bbox_max[j] = std::max(bbox_max[j], bbox[j]);
          rowindofcol[blk_select_idx[block_id] + i].push_back(rhs.size() + j);
          constrvalues[blk_select_idx[block_id] + i].push_back(bbox[j]);
        }
        bbox_max[4] = std::max(bbox_max[4], bbox[4]);
        bbox_max[5] = std::max(bbox_max[5], bbox[5]);
        deltaxid.push_back(bbox[4]);
        deltayid.push_back(bbox[5]);
      }
      for (unsigned j = 0; j < 4; ++j) {
        collb[std::get<0>(it.second) + j] = 0;
        colub[std::get<0>(it.second) + j] = bbox_max[j];
        rowindofcol[std::get<0>(it.second)  + j].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + j].push_back(-1);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('p');
      }
      if (samedeltax || deltaxneg) {
        std::get<1>(it.second) = deltax;
      } else {
        for (unsigned j = 0; j < deltaxid.size(); ++j) {
          rowindofcol[blk_select_idx[block_id] + j].push_back(rhs.size());
          constrvalues[blk_select_idx[block_id] + j].push_back(deltaxid[j]);
        }
        rowindofcol[std::get<0>(it.second)  + 4].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 4].push_back(-1);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('p');
        collb[std::get<0>(it.second) + 4] = (deltaxneg ? -infty : 0);
        colub[std::get<0>(it.second) + 4] = bbox_max[4];
        rowindofcol[std::get<0>(it.second)  + 6].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 6].push_back(1);
        rowindofcol[std::get<0>(it.second)  + 4].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 4].push_back(-1);
        sens.push_back('L');
        rhs.push_back(0);
        rowtype.push_back('p');
        rowindofcol[std::get<0>(it.second)  + 6].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 6].push_back(1);
        rowindofcol[block_id * 6  + 2].push_back(rhs.size());
        constrvalues[block_id * 6 + 2].push_back(-bbox_max[4]);
        sens.push_back('L');
        rhs.push_back(0);
        rowtype.push_back('p');
        rowindofcol[std::get<0>(it.second)  + 6].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 6].push_back(1);
        rowindofcol[std::get<0>(it.second)  + 4].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 4].push_back(-1);
        rowindofcol[block_id * 6  + 2].push_back(rhs.size());
        constrvalues[block_id * 6 + 2].push_back(-bbox_max[4]);
        sens.push_back('G');
        rhs.push_back(-bbox_max[4]);
        rowtype.push_back('p');
      }
      if (samedeltay || deltayneg) {
        std::get<2>(it.second) = deltay;
      } else {
        for (unsigned j = 0; j < deltayid.size(); ++j) {
          rowindofcol[blk_select_idx[block_id] + j].push_back(rhs.size());
          constrvalues[blk_select_idx[block_id] + j].push_back(deltayid[j]);
        }
        rowindofcol[std::get<0>(it.second)  + 5].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 5].push_back(-1);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('p');
        collb[std::get<0>(it.second) + 5] = (deltayneg ? -infty : 0);
        colub[std::get<0>(it.second) + 5] = bbox_max[5];
        rowindofcol[std::get<0>(it.second)  + 7].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 7].push_back(1);
        rowindofcol[std::get<0>(it.second)  + 5].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 5].push_back(-1);
        sens.push_back('L');
        rhs.push_back(0);
        rowtype.push_back('p');
        rowindofcol[std::get<0>(it.second)  + 7].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 7].push_back(1);
        rowindofcol[block_id * 6  + 3].push_back(rhs.size());
        constrvalues[block_id * 6 + 3].push_back(-bbox_max[5]);
        sens.push_back('L');
        rhs.push_back(0);
        rowtype.push_back('p');
        rowindofcol[std::get<0>(it.second)  + 7].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 7].push_back(1);
        rowindofcol[std::get<0>(it.second)  + 5].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + 5].push_back(-1);
        rowindofcol[block_id * 6  + 3].push_back(rhs.size());
        constrvalues[block_id * 6 + 3].push_back(-bbox_max[5]);
        sens.push_back('G');
        rhs.push_back(-bbox_max[5]);
        rowtype.push_back('p');
      }
    } else {
      const auto& blk = mydesign.Blocks[block_id][0];
      int bbox[6];
      bbox[0] = blk.width / 2,  bbox[2] = blk.width / 2;
      bbox[1] = blk.height / 2, bbox[3] = blk.height / 2;
      if (blk.blockPins.size()) {
        bbox[0] = blk.blockPins[pin_id].bbox.LL.x;
        bbox[1] = blk.blockPins[pin_id].bbox.LL.y;
        bbox[2] = blk.blockPins[pin_id].bbox.UR.x;
        bbox[3] = blk.blockPins[pin_id].bbox.UR.y;
      }
      bbox[4] = blk.width  - bbox[0] - bbox[2];
      bbox[5] = blk.height - bbox[1] - bbox[3];

      for (unsigned j = 0; j < 4; ++j) {
        collb[std::get<0>(it.second) + j] = bbox[j];
        colub[std::get<0>(it.second) + j] = bbox[j];
        rowindofcol[std::get<0>(it.second) + j].push_back(rhs.size());
        constrvalues[std::get<0>(it.second) + j].push_back(1);
        sens.push_back('E');
        rhs.push_back(bbox[j]);
        rowtype.push_back('p');
      }
      std::get<1>(it.second) = bbox[4];
      std::get<2>(it.second) = bbox[5];
    }
  }

  // ordering constraint
  std::set<std::pair<int, int>> ordering_h, ordering_v;
  for (const auto& it : mydesign.Ordering_Constraints) {
    if (it.second == placerDB::H) {
      ordering_h.emplace(it.first);
    } else {
      ordering_v.emplace(it.first);
    }
  }


  std::set<std::pair<int, int>> abut_h, abut_v;
  for (const auto& it : mydesign.Abut_Constraints) {
    if (it.second == placerDB::H) {
      abut_h.emplace(it.first);
    } else {
      abut_v.emplace(it.first);
    }
  }
  for (int v : {1, 0}) {
    const auto& bias = v ? bias_Vgraph : bias_Hgraph;
    for (const auto& it : (v ? ordering_v : ordering_h)) {
      auto itabut = (v ? abut_v.find(it) : abut_h.find(it));
      if ((v && abut_v.find(it) != abut_v.end()) || (!v && abut_h.find(it) != abut_h.end())) {
        continue;
      }
      const auto& i = v ? it.second : it.first;
      const auto& j = v ? it.first  : it.second;
      rowindofcol[i * 6 + v].push_back(rhs.size());
      rowindofcol[i * 6 + 4 + v].push_back(rhs.size());
      rowindofcol[j * 6 + v].push_back(rhs.size());
      constrvalues[i * 6 + v].push_back(1);
      constrvalues[i * 6 + 4 + v].push_back(1);
      constrvalues[j * 6 + v].push_back(-1);
      sens.push_back('L');
      rhs.push_back(-bias);
      rowtype.push_back('v');
    }
    for (const auto& it : (v ? abut_v : abut_h)) {
      const auto& i = it.first;
      const auto& j = it.second;
      rowindofcol[i * 6 + v].push_back(rhs.size());
      rowindofcol[i * 6 + 4 + v].push_back(rhs.size());
      rowindofcol[j * 6 + v].push_back(rhs.size());
      constrvalues[i * 6 + v].push_back(1);
      constrvalues[i * 6 + 4 + v].push_back(1);
      constrvalues[j * 6 + v].push_back(-1);
      sens.push_back('E');
      rhs.push_back(0);
      rowtype.push_back('a');
    }
  }

  for (const auto& group : mydesign.Same_Template_Constraints) {
    auto it1 = group.begin();
    auto it2 = std::next(it1);
    while (it2 != group.end()) {
      if (mydesign.Blocks[*it1].size() == mydesign.Blocks[*it2].size()) {
        for (unsigned idx1 = blk_select_idx[*it1], idx2 = blk_select_idx[*it2];
            idx1 < (blk_select_idx[*it1] + mydesign.Blocks[*it1].size());
            ++idx1, ++idx2) {
          rowindofcol[idx1].push_back(rhs.size());
          rowindofcol[idx2].push_back(rhs.size());
          constrvalues[idx1].push_back(1);
          constrvalues[idx2].push_back(-1);
          sens.push_back('E');
          rhs.push_back(0);
          rowtype.push_back('e');
        }
      }
      it1 = it2++;
    }
  }

  // area variables Area_x >= x_i + w_i, Area_y >= y_i + h_i for all blocks {i}
  for (unsigned int i = 0; i < mydesign.Blocks.size(); i++) {
    rowindofcol[i * 6].push_back(rhs.size());
    rowindofcol[N_area_max - 2].push_back(rhs.size());
    constrvalues[i * 6].push_back(1);
    constrvalues[N_area_max - 2].push_back(-1);
    if (flushbl) {
      rowindofcol[i * 6 + 4].push_back(rhs.size());
      constrvalues[i * 6 + 4].push_back(1);
      sens.push_back('L');
    } else {
      sens.push_back('G');
    }
    rhs.push_back(0);
    rowtype.push_back('A');

    rowindofcol[i * 6 + 1].push_back(rhs.size());
    rowindofcol[N_area_max - 1].push_back(rhs.size());
    constrvalues[i * 6 + 1].push_back(1);
    constrvalues[N_area_max - 1].push_back(-1);
    if (flushbl) {
      rowindofcol[i * 6 + 5].push_back(rhs.size());
      constrvalues[i * 6 + 5].push_back(1);
      sens.push_back('L');
    } else {
      sens.push_back('G');
    }
    rhs.push_back(0);
    rowtype.push_back('A');
  }


  // symmetry block constraint
  for (const auto& SPBlock : mydesign.SPBlocks) {
    std::set<std::pair<int, int>> sympair(SPBlock.sympair.begin(), SPBlock.sympair.end());
    std::set<int> selfsym;
    for (const auto& it : SPBlock.selfsym) {
      selfsym.insert(it.first);
    }
    if (SPBlock.axis_dir == placerDB::H) {
      // constraint inside one pair
      for (const auto& sp : sympair) {
        int first_id = sp.first, second_id = sp.second;
        // each pair has opposite V flip
        {
          rowindofcol[first_id   * 6 + 3].push_back(rhs.size());
          rowindofcol[second_id  * 6 + 3].push_back(rhs.size());
          constrvalues[first_id  * 6 + 3].push_back(1);
          constrvalues[second_id * 6 + 3].push_back(1);
          sens.push_back('E');
          rhs.push_back(1);
          rowtype.push_back('s');
        }
        // each pair has the same H flip
        {
          rowindofcol[first_id   * 6 + 2].push_back(rhs.size());
          rowindofcol[second_id  * 6 + 2].push_back(rhs.size());
          constrvalues[first_id  * 6 + 2].push_back(1);
          constrvalues[second_id * 6 + 2].push_back(-1);
          sens.push_back('E');
          rhs.push_back(0);
          rowtype.push_back('s');
        }
        // x center of blocks in each pair are the same
        {
          rowindofcol[first_id   * 6].push_back(rhs.size());
          rowindofcol[second_id  * 6].push_back(rhs.size());
          rowindofcol[first_id   * 6 + 4].push_back(rhs.size());
          rowindofcol[second_id  * 6 + 4].push_back(rhs.size());
          constrvalues[first_id  * 6].push_back(1);
          constrvalues[second_id * 6].push_back(-1);
          constrvalues[first_id  * 6 + 4].push_back(0.5);
          constrvalues[second_id * 6 + 4].push_back(-0.5);
          sens.push_back('E');
          rhs.push_back(0);
          rowtype.push_back('s');
        }
      }

      // constraint between two pairs
      for (auto i = sympair.begin(); i != sympair.end(); ++i) {
        int i_first_id = i->first, i_second_id = i->second;
        auto j = std::next(i);
        if (j == sympair.end()) break;
        // the y center of the two pairs are the same
        int j_first_id = j->first, j_second_id = j->second;
        rowindofcol[i_first_id  * 6 + 1].push_back(rhs.size());
        rowindofcol[i_second_id * 6 + 1].push_back(rhs.size());
        rowindofcol[j_first_id  * 6 + 1].push_back(rhs.size());
        rowindofcol[j_second_id * 6 + 1].push_back(rhs.size());
        rowindofcol[i_first_id  * 6 + 5].push_back(rhs.size());
        rowindofcol[i_second_id * 6 + 5].push_back(rhs.size());
        rowindofcol[j_first_id  * 6 + 5].push_back(rhs.size());
        rowindofcol[j_second_id * 6 + 5].push_back(rhs.size());
        constrvalues[i_first_id  * 6 + 1].push_back(0.5);
        constrvalues[i_second_id * 6 + 1].push_back(0.5);
        constrvalues[j_first_id  * 6 + 1].push_back(-0.5);
        constrvalues[j_second_id * 6 + 1].push_back(-0.5);
        constrvalues[i_first_id  * 6 + 5].push_back(0.25);
        constrvalues[i_second_id * 6 + 5].push_back(0.25);
        constrvalues[j_first_id  * 6 + 5].push_back(-0.25);
        constrvalues[j_second_id * 6 + 5].push_back(-0.25);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('s');
      }
      // constraint between two selfsyms
      for (auto i = selfsym.begin(); i != selfsym.end(); ++i) {
        int i_id = *i;
        auto j = std::next(i);
        if (j == selfsym.end()) break;
        // the y center of the two selfsyms are the same
        int j_id = *j;
        rowindofcol[i_id * 6 + 1].push_back(rhs.size());
        rowindofcol[j_id * 6 + 1].push_back(rhs.size());
        rowindofcol[i_id * 6 + 5].push_back(rhs.size());
        rowindofcol[j_id * 6 + 5].push_back(rhs.size());
        constrvalues[i_id * 6 + 1].push_back(1);
        constrvalues[j_id * 6 + 1].push_back(-1);
        constrvalues[i_id * 6 + 5].push_back(0.5);
        constrvalues[j_id * 6 + 5].push_back(-0.5);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('s');
      }
      if (!sympair.empty() && !selfsym.empty()) {
        // constraint between a pair and a selfsym
        const auto& i = *(sympair.begin());
        int i_first_id = i.first, i_second_id = i.second;
        int j_id = *(selfsym.begin());
        // the y center of the pair and the selfsym are the same
        rowindofcol[i_first_id  * 6 + 1].push_back(rhs.size());
        rowindofcol[i_second_id * 6 + 1].push_back(rhs.size());
        rowindofcol[j_id * 6 + 1].push_back(rhs.size());
        rowindofcol[i_first_id  * 6 + 5].push_back(rhs.size());
        rowindofcol[i_second_id * 6 + 5].push_back(rhs.size());
        rowindofcol[j_id * 6 + 5].push_back(rhs.size());
        constrvalues[i_first_id  * 6 + 1].push_back(0.5);
        constrvalues[i_second_id * 6 + 1].push_back(0.5);
        constrvalues[j_id * 6 + 1].push_back(-1);
        constrvalues[i_first_id  * 6 + 5].push_back(0.25);
        constrvalues[i_second_id * 6 + 5].push_back(0.25);
        constrvalues[j_id * 6 + 5].push_back(-0.5);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('s');
      }
    } else {
      // axis_dir==V
      // constraint inside one pair
      for (const auto& sp : sympair) {
        int first_id = sp.first, second_id = sp.second;
        // each pair has opposite H flip
        {
          rowindofcol[first_id   * 6 + 2].push_back(rhs.size());
          rowindofcol[second_id  * 6 + 2].push_back(rhs.size());
          constrvalues[first_id  * 6 + 2].push_back(1);
          constrvalues[second_id * 6 + 2].push_back(1);
          sens.push_back('E');
          rhs.push_back(1);
          rowtype.push_back('s');
        }
        // each pair has the same V flip
        {
          rowindofcol[first_id   * 6 + 3].push_back(rhs.size());
          rowindofcol[second_id  * 6 + 3].push_back(rhs.size());
          constrvalues[first_id  * 6 + 3].push_back(1);
          constrvalues[second_id * 6 + 3].push_back(-1);
          sens.push_back('E');
          rhs.push_back(0);
          rowtype.push_back('s');
        }
        // y center of blocks in each pair are the same
        {
          rowindofcol[first_id   * 6 + 1].push_back(rhs.size());
          rowindofcol[second_id  * 6 + 1].push_back(rhs.size());
          rowindofcol[first_id   * 6 + 5].push_back(rhs.size());
          rowindofcol[second_id  * 6 + 5].push_back(rhs.size());
          constrvalues[first_id  * 6 + 1].push_back(1);
          constrvalues[second_id * 6 + 1].push_back(-1);
          constrvalues[first_id  * 6 + 5].push_back(0.5);
          constrvalues[second_id * 6 + 5].push_back(-0.5);
          sens.push_back('E');
          rhs.push_back(0);
          rowtype.push_back('s');
        }
      }

      // constraint between two pairs
      for (auto i = sympair.begin(); i != sympair.end(); ++i) {
        int i_first_id = i->first, i_second_id = i->second;
        auto j = std::next(i);
        if (j == sympair.end()) break;
        // the x center of the two pairs are the same
        int j_first_id = j->first, j_second_id = j->second;
        rowindofcol[i_first_id  * 6].push_back(rhs.size());
        rowindofcol[i_second_id * 6].push_back(rhs.size());
        rowindofcol[j_first_id  * 6].push_back(rhs.size());
        rowindofcol[j_second_id * 6].push_back(rhs.size());
        rowindofcol[i_first_id  * 6 + 4].push_back(rhs.size());
        rowindofcol[i_second_id * 6 + 4].push_back(rhs.size());
        rowindofcol[j_first_id  * 6 + 4].push_back(rhs.size());
        rowindofcol[j_second_id * 6 + 4].push_back(rhs.size());
        constrvalues[i_first_id  * 6].push_back(0.5);
        constrvalues[i_second_id * 6].push_back(0.5);
        constrvalues[j_first_id  * 6].push_back(-0.5);
        constrvalues[j_second_id * 6].push_back(-0.5);
        constrvalues[i_first_id  * 6 + 4].push_back(0.25);
        constrvalues[i_second_id * 6 + 4].push_back(0.25);
        constrvalues[j_first_id  * 6 + 4].push_back(-0.25);
        constrvalues[j_second_id * 6 + 4].push_back(-0.25);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('s');
      }
      // constraint between two selfsyms
      for (auto i = selfsym.begin(); i != selfsym.end(); ++i) {
        int i_id = *i;
        auto j = std::next(i);
        if (j == selfsym.end()) break;
        // the x center of the two selfsyms are the same
        int j_id = *j;
        rowindofcol[i_id * 6].push_back(rhs.size());
        rowindofcol[j_id * 6].push_back(rhs.size());
        rowindofcol[i_id * 6 + 4].push_back(rhs.size());
        rowindofcol[j_id * 6 + 4].push_back(rhs.size());
        constrvalues[i_id * 6].push_back(1);
        constrvalues[j_id * 6].push_back(-1);
        constrvalues[i_id * 6 + 4].push_back(0.5);
        constrvalues[j_id * 6 + 4].push_back(-0.5);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('s');
      }
      if (!sympair.empty() && !selfsym.empty()) {
        // constraint between a pair and a selfsym
        const auto& i = *sympair.begin();
        int i_first_id = i.first, i_second_id = i.second;
        int j_id = *selfsym.begin();
        // the x center of the pair and the selfsym are the same
        rowindofcol[i_first_id  * 6].push_back(rhs.size());
        rowindofcol[i_second_id * 6].push_back(rhs.size());
        rowindofcol[j_id * 6].push_back(rhs.size());
        rowindofcol[i_first_id  * 6 + 4].push_back(rhs.size());
        rowindofcol[i_second_id * 6 + 4].push_back(rhs.size());
        rowindofcol[j_id * 6 + 4].push_back(rhs.size());
        constrvalues[i_first_id  * 6].push_back(0.5);
        constrvalues[i_second_id * 6].push_back(0.5);
        constrvalues[j_id * 6].push_back(-1);
        constrvalues[i_first_id  * 6 + 4].push_back(0.25);
        constrvalues[i_second_id * 6 + 4].push_back(0.25);
        constrvalues[j_id * 6 + 4].push_back(-0.5);
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('s');
      }
    } 
  }

  // align block constraint
  for (const auto& alignment_unit : mydesign.Align_blocks) {
    for (unsigned int j = 0; j < alignment_unit.blocks.size() - 1; j++) {
      int first_id = alignment_unit.blocks[j], second_id = alignment_unit.blocks[j + 1];
      if (alignment_unit.horizon == 1) {
        rowindofcol[first_id  * 6 + 1].push_back(rhs.size());
        rowindofcol[second_id * 6 + 1].push_back(rhs.size());
        constrvalues[first_id  * 6 + 1].push_back(1);
        constrvalues[second_id * 6 + 1].push_back(-1);
        if (alignment_unit.line == 1) {
          // align center y
          rowindofcol[first_id  * 6 + 5].push_back(rhs.size());
          rowindofcol[second_id * 6 + 5].push_back(rhs.size());
          constrvalues[first_id  * 6 + 5].push_back(0.5);
          constrvalues[second_id * 6 + 5].push_back(-0.5);
        } else if (alignment_unit.line == 2) {
          // align to top
          rowindofcol[first_id  * 6 + 5].push_back(rhs.size());
          rowindofcol[second_id * 6 + 5].push_back(rhs.size());
          constrvalues[first_id  * 6 + 5].push_back(1);
          constrvalues[second_id * 6 + 5].push_back(-1);
        }
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('l');
      } else {
        rowindofcol[first_id  * 6].push_back(rhs.size());
        rowindofcol[second_id * 6].push_back(rhs.size());
        constrvalues[first_id  * 6].push_back(1);
        constrvalues[second_id * 6].push_back(-1);
        if (alignment_unit.line == 1) {
          // align center x
          rowindofcol[first_id  * 6 + 4].push_back(rhs.size());
          rowindofcol[second_id * 6 + 4].push_back(rhs.size());
          constrvalues[first_id  * 6 + 4].push_back(0.5);
          constrvalues[second_id * 6 + 4].push_back(-0.5);
        } else if (alignment_unit.line == 2) {
          // align to right
          rowindofcol[first_id  * 6 + 4].push_back(rhs.size());
          rowindofcol[second_id * 6 + 4].push_back(rhs.size());
          constrvalues[first_id  * 6 + 4].push_back(1);
          constrvalues[second_id * 6 + 4].push_back(-1);
        }
        sens.push_back('E');
        rhs.push_back(0);
        rowtype.push_back('l');
      }
    }
  }

  // add HPWL in cost
  // hpwl_xmin_i <= pin_j_xmin, hpwl_ymin_i <= pin_j_ymin
  // hpwl_xmax_i >= pin_j_xmax, hpwl_ymax_u >= pin_j_ymax
  // pin_j_xmin = block_j_xmin + pin_xmin if no flip
  //            = block_j_xmin + w_j - pin_xmax if flipped
  // combining the two:
  // pin_j_xmin = block_y_xmin + flip_j * (w_j - pin_xmax - pin_xmin) + pin_xmin
  // -do- for ymin
  for (unsigned int i = 0; i < mydesign.Nets.size(); i++) {
    if (mydesign.Nets[i].connected.size() < 2) continue;

    int cnt{0};
    for (unsigned int j = 0; j < mydesign.Nets[i].connected.size(); j++) {
      if (mydesign.Nets[i].connected[j].type == placerDB::Block) {
        ++cnt;
      }
    }
    if (cnt <2) continue;

    int ind = int(N_block_vars_max + i * 4);
    for (unsigned int j = 0; j < mydesign.Nets[i].connected.size(); j++) {
      if (mydesign.Nets[i].connected[j].type == placerDB::Block) {
        const int block_id = mydesign.Nets[i].connected[j].iter2;
        const int pin_id = mydesign.Nets[i].connected[j].iter;
        auto it = pin_idx_map.find(std::make_pair(block_id, pin_id));
        if (it == pin_idx_map.end()) continue;
        int llx = std::get<0>(it->second),     lly = std::get<0>(it->second) + 1;
        int urx = std::get<0>(it->second) + 2, ury = std::get<0>(it->second) + 3;
        const auto& blk = mydesign.Blocks[block_id][curr_sp.selected[block_id]];
        {
          rowindofcol[block_id * 6].push_back(rhs.size());
          rowindofcol[llx].push_back(rhs.size());
          rowindofcol[ind].push_back(rhs.size());
          constrvalues[block_id * 6].push_back(1);
          constrvalues[llx].push_back(1);
          constrvalues[ind].push_back(-1);
          if (std::get<1>(it->second) != INT_MIN) {
            rowindofcol[block_id * 6 + 2].push_back(rhs.size());
            constrvalues[block_id * 6 + 2].push_back(std::get<1>(it->second));
          } else {
            rowindofcol[std::get<0>(it->second) + 6].push_back(rhs.size());
            constrvalues[std::get<0>(it->second) + 6].push_back(1);
          }
          sens.push_back('G');
          rhs.push_back(0);
          rowtype.push_back('h');
        }
        {
          rowindofcol[block_id * 6 + 1].push_back(rhs.size());
          rowindofcol[lly].push_back(rhs.size());
          rowindofcol[ind + 1].push_back(rhs.size());
          constrvalues[block_id * 6 + 1].push_back(1);
          constrvalues[lly].push_back(1);
          constrvalues[ind + 1].push_back(-1);
          if (std::get<2>(it->second) != INT_MIN) {
            rowindofcol[block_id * 6 + 3].push_back(rhs.size());
            constrvalues[block_id * 6 + 3].push_back(std::get<2>(it->second));
          } else {
            rowindofcol[std::get<0>(it->second) + 7].push_back(rhs.size());
            constrvalues[std::get<0>(it->second) + 7].push_back(1);
          }
          sens.push_back('G');
          rhs.push_back(0);
          rowtype.push_back('h');
        }
        {
          rowindofcol[block_id * 6].push_back(rhs.size());
          rowindofcol[urx].push_back(rhs.size());
          rowindofcol[ind + 2].push_back(rhs.size());
          constrvalues[block_id * 6].push_back(1);
          constrvalues[urx].push_back(1);
          constrvalues[ind + 2].push_back(-1);
          if (std::get<1>(it->second) != INT_MIN) {
            rowindofcol[block_id * 6 + 2].push_back(rhs.size());
            constrvalues[block_id * 6 + 2].push_back(std::get<1>(it->second));
          } else {
            rowindofcol[std::get<0>(it->second) + 6].push_back(rhs.size());
            constrvalues[std::get<0>(it->second) + 6].push_back(1);
          }
          sens.push_back('L');
          rhs.push_back(0);
          rowtype.push_back('h');
        }
        {
          rowindofcol[block_id * 6 + 1].push_back(rhs.size());
          rowindofcol[ury].push_back(rhs.size());
          rowindofcol[ind + 3].push_back(rhs.size());
          constrvalues[block_id * 6 + 1].push_back(1);
          constrvalues[ury].push_back(1);
          constrvalues[ind + 3].push_back(-1);
          if (std::get<2>(it->second) != INT_MIN) {
            rowindofcol[block_id * 6 + 3].push_back(rhs.size());
            constrvalues[block_id * 6 + 3].push_back(std::get<2>(it->second));
          } else {
            rowindofcol[std::get<0>(it->second) + 7].push_back(rhs.size());
            constrvalues[std::get<0>(it->second) + 7].push_back(1);
          }
          sens.push_back('L');
          rhs.push_back(0);
          rowtype.push_back('h');
        }
      }
    }
  }
  area_ilp = 0.;
  HPWL_ILP = 0.;
  {
    std::vector<int> starts, indices;
    std::vector<double> values;
    starts.push_back(0);
    assert(rhs.size() == sens.size());
    for (int i = 0; i < N_var; ++i) {
      starts.push_back(starts.back() + rowindofcol[i].size());
      indices.insert(indices.end(), rowindofcol[i].begin(), rowindofcol[i].end());
      values.insert(values.end(), constrvalues[i].begin(), constrvalues[i].end());
    }
    double rhslb[rhs.size()], rhsub[rhs.size()];
    for (unsigned i = 0;i < sens.size(); ++i) {
      switch (sens[i]) {
        case 'E':
        default:
          rhslb[i] = rhs[i];
          rhsub[i] = rhs[i];
          break;
        case 'G':
          rhslb[i] = rhs[i];
          rhsub[i] = infty;
          break;
        case 'L':
          rhslb[i] = -infty;
          rhsub[i] = rhs[i];
          break;
      }
    }

    osiclp.loadProblem(N_var, (int)rhs.size(), starts.data(), indices.data(),
        values.data(), collb.data(), colub.data(),
        objective.data(), rhslb, rhsub);
    for (int i = 0; i < intvars.size(); ++i) {
      if (intvars[i]) {
        osiclp.setInteger(i);
      }
    }
    //sym_set_int_param(env, "gap_limit", (getenv("SYM_GAP_LIMIT") ? std::atoi(getenv("SYM_GAP_LIMIT")) : -1));

    static int write_cnt{0};
    static std::string block_name;
    if (block_name != mydesign.name) {
      write_cnt = 0;
      block_name = mydesign.name;
    }
    if (write_cnt < 10) {
      // add var names if writing to lp file
      char* names[N_var];
      std::vector<std::string> namesvec(N_var);
      for (int i = 0; i < mydesign.Blocks.size(); i++) {
        int ind = i * 6;
        namesvec[ind]     = (mydesign.Blocks[i][0].name + "_x\0");
        names[ind] = &(namesvec[ind][0]);
        namesvec[ind + 1] = (mydesign.Blocks[i][0].name + "_y\0");
        names[ind + 1] = &(namesvec[ind + 1][0]);
        namesvec[ind + 2] = (mydesign.Blocks[i][0].name + "_flx\0");
        names[ind + 2] = &(namesvec[ind + 2][0]);
        namesvec[ind + 3] = (mydesign.Blocks[i][0].name + "_fly\0");
        names[ind + 3] = &(namesvec[ind + 3][0]);
        namesvec[ind + 4] = (mydesign.Blocks[i][0].name + "_width\0");
        names[ind + 4] = &(namesvec[ind + 4][0]);
        namesvec[ind + 5] = (mydesign.Blocks[i][0].name + "_height\0");
        names[ind + 5] = &(namesvec[ind + 5][0]);
      }

      for (int i = 0; i < mydesign.Nets.size(); ++i) {
        int ind = i * 4 + N_block_vars_max;
        namesvec[ind]     = (mydesign.Nets[i].name + "_ll_x\0");
        names[ind] = &(namesvec[ind][0]);
        namesvec[ind + 1] = (mydesign.Nets[i].name + "_ll_y\0");
        names[ind + 1] = &(namesvec[ind + 1][0]);
        namesvec[ind + 2] = (mydesign.Nets[i].name + "_ur_x\0");
        names[ind + 2] = &(namesvec[ind + 2][0]);
        namesvec[ind + 3] = (mydesign.Nets[i].name + "_ur_y\0");
        names[ind + 3] = &(namesvec[ind + 3][0]);
      }

      for (auto& it : buf_indx_map) {
        namesvec[it.second] = (mydesign.Blocks[it.first.first][0].name + "__" + mydesign.Blocks[it.first.second][0].name + "_buf\0");
        names[it.second]    = &(namesvec[it.second][0]);
      }
      for (auto& it : buf_xy_indx_map) {
        namesvec[it.second] = (mydesign.Blocks[it.first.first][0].name + "__" + mydesign.Blocks[it.first.second][0].name + "_buf_xy\0");
        names[it.second] = &(namesvec[it.second][0]);
      }
      unsigned idx = blk_select_idx[0];
      for (const auto& blk : mydesign.Blocks) {
        if (blk.size() <= 1) continue;
        for (unsigned j = 0; j < blk.size(); ++j) {
          namesvec[idx] = (blk[j].name + "_select_" + std::to_string(j) + "\0");
          names[idx]    = &(namesvec[idx][0]);
          ++idx;
        }
      }

      std::string strvec[] = {"_llx\0", "_lly\0", "_urx\0", "_ury\0", "_deltax\0", "_deltay\0", "_auxx\0", "_auxy\0"};
      for (const auto& it : pin_idx_map) {
        const auto& blk = mydesign.Blocks[it.first.first][0];
        const auto& pin_id = it.first.second;
        for (unsigned i = 0; i < (mydesign.Blocks[it.first.first].size() > 1 ? 8 : 4); ++i) {
          namesvec[std::get<0>(it.second) + i] = (blk.name + "_pin_" + blk.blockPins[pin_id].name + strvec[i]);
          names[std::get<0>(it.second) + i]    = &(namesvec[std::get<0>(it.second) + i][0]);
        }
      }

      namesvec[N_area_max - 1] = (mydesign.name + "_area_y\0");
      names[N_area_max - 1]    = &(namesvec[N_area_max - 1][0]);
      namesvec[N_area_max - 2] = (mydesign.name + "_area_x\0");
      names[N_area_max - 2]    = &(namesvec[N_area_max - 2][0]);

      namesvec[N_aspect_ratio_max - 1] = (mydesign.name + "_aspect_p\0");
      names[N_aspect_ratio_max - 1]    = &(namesvec[N_aspect_ratio_max - 1][0]);
      namesvec[N_aspect_ratio_max - 2] = (mydesign.name + "_aspect_n\0");
      names[N_aspect_ratio_max - 2]    = &(namesvec[N_aspect_ratio_max - 2][0]);

      for (unsigned i = 0; i < N_var; ++i) {
        osiclp.setColName(i, names[i]);
      }
      for (unsigned i = 0; i < rhs.size(); ++i) {
        osiclp.setRowName(i, (rowtype[i] + std::to_string(i)).c_str());
      }
      osiclp.writeLp(const_cast<char*>((mydesign.name + "_ilp_" + std::to_string(write_cnt) + ".lp").c_str()));
      ++write_cnt;
    }
    //solve the integer program
    CbcModel model(osiclp);
    int status{0};
    {
      TimeMeasure tm(const_cast<design&>(mydesign).ilp_solve_runtime);
      model.setLogLevel(0);
      model.setMaximumSeconds(500);
      if (num_threads > 0 && CbcModel::haveMultiThreadSupport()) {
        model.setNumberThreads(num_threads);
        model.setMaximumSeconds(500 * num_threads);
        const char* argv[] = {"", "-threads", std::to_string(num_threads).c_str(), "-solve"};
        status = CbcMain1(4, argv, model);
      } else {
        const char* argv[] = {"", "-solve"};
        status = CbcMain1(2, argv, model);
      }
      logger->info("status : {0} {1}" , CbcModel::haveMultiThreadSupport(), num_threads);
      //model.branchAndBound();
    }
    //int status = model.secondaryStatus();
    const double* var = model.bestSolution();
    logger->info("status : {0}", status);
    if (status != 0 || !var) {
      ++const_cast<design&>(mydesign)._infeasILPFail;
      return false;
    }
    int minx(INT_MAX), miny(INT_MAX);
    area_ilp = (var[N_area_max - 1] * var[N_area_max - 2]);
    for (int i = 0; i < mydesign.Blocks.size(); i++) {
      Blocks[i].x = roundupint(var[i * 6]);
      Blocks[i].y = roundupint(var[i * 6 + 1]);
      minx = std::min(minx, Blocks[i].x);
      miny = std::min(miny, Blocks[i].y);
      Blocks[i].H_flip = roundupint(var[i * 6 + 2]);
      Blocks[i].V_flip = roundupint(var[i * 6 + 3]);
      if (mydesign.Blocks[i].size() > 1) {
        int select{-1};
        for (int j = 0; j < mydesign.Blocks[i].size(); ++j) {
          if (roundupint(var[blk_select_idx[i] + j]) > 0.5) {
            select = j;
            break;
          }
        }
        if (select >= 0) {
          const_cast<SeqPair&>(curr_sp).selected[i] = select;
        }
      }
    }
    for (int i = 0; i < mydesign.Blocks.size(); i++) {
      Blocks[i].x -= minx;
      Blocks[i].y -= miny;
    }
    // calculate HPWL from ILP solution
    for (int i = 0; i < mydesign.Nets.size(); ++i) {
      int ind = int(N_block_vars_max + i * 4);
      HPWL_ILP += (var[ind + 3] + var[ind + 2] - var[ind + 1] - var[ind]);
    }
  }

  return true;
}

