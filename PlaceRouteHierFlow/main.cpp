#include "main.h"

double ConstGraph::LAMBDA=1000;
double ConstGraph::GAMAR=30;
double ConstGraph::BETA=100;
double ConstGraph::SIGMA=1000;
double ConstGraph::PHI=1500;

int main(int argc, char** argv ){

  //
  // Enable or disable state saving in json at intermediate points
  // Currently adds 4 seconds to a 29 second baseline for the switched_capacitor_filter
  // And generates 69MB in files
  bool skip_saving_state = getenv( "PNRDB_SAVE_STATE") == NULL;
  bool adr_mode = getenv( "PNRDB_ADR_MODE") != NULL;
  bool disable_io = getenv( "PNRDB_disable_io") != NULL;; //turn off window outputs
  bool multi_thread = getenv( "PNRDB_multi_thread") != NULL;;  // run multi layouts in multi threads
  //bool disable_io = false; //turn off window outputs
  //bool multi_thread = false;  // run multi layouts in multi threads

  string opath="./Results/";
  string fpath=argv[1];
  string lfile=argv[2];
  string vfile=argv[3];
  string mfile=argv[4];
  string dfile=argv[5];
  string topcell=argv[6];
  int numLayout=std::stoi(argv[7]);
  int effort=std::stoi(argv[8]);
  if(fpath.back()=='/') {fpath.erase(fpath.end()-1);}
  if(opath.back()!='/') {opath+="/";}

  // Following codes try to get the path of binary codes
  string binary_directory = argv[0];
  cout <<"argv[0]: "<<binary_directory <<endl;
  int beginIdx = binary_directory.rfind('/');//find the last slash
  string str_lastOne = binary_directory.substr(beginIdx+1);
  cout <<"string after last slash: "<<str_lastOne <<endl;
  binary_directory = binary_directory.erase(beginIdx+1);
  cout <<"binary_directory: "<< binary_directory <<endl;

  mkdir(opath.c_str(), 0777);

  PnRdatabase DB(fpath, topcell, vfile, lfile, mfile, dfile); // construction of database
  PnRDB::Drc_info drcInfo=DB.getDrc_info();
  map<string, PnRDB::lefMacro> lefData = DB.checkoutSingleLEF();


  if ( !skip_saving_state) {
    deque<int> Q=DB.TraverseHierTree(); // traverse hierarchical tree in topological order
    json jsonStrAry = json::array();
    std::ofstream jsonStream;
    jsonStream.open( opath + "__hierTree.json");
    while (!Q.empty()) {
      jsonStrAry.push_back( DB.CheckoutHierNode(Q.front()).name);
      Q.pop_front();
    }
    jsonStream << std::setw(4) << jsonStrAry;
    jsonStream.close();
  }

  deque<int> Q = DB.TraverseHierTree();  // traverse hierarchical tree in topological order
  std::vector<int> TraverseOrder;        // save traverse order, same as Q
  int Q_size = Q.size();
  for (int i = 0; i < Q_size; i++) {  // copy Q to TraverseOrder
    TraverseOrder.push_back(Q.front());
    Q.pop_front();
    Q.push_back(TraverseOrder.back());
  }

  for (int i = 0; i < Q_size;i++)
  {
    int idx=TraverseOrder[i];
    cout<<"Main-Info: start to work on node "<<idx<<endl;
    if(disable_io)std::cout.setstate(std::ios_base::failbit);
    PnRDB::hierNode current_node=DB.CheckoutHierNode(idx);
    DB.PrintHierNode(current_node);

    
    DB.AddingPowerPins(current_node);
    Placer_Router_Cap PRC(opath, fpath, current_node, drcInfo, lefData, 1, 6); //dummy, aspect ratio, number of aspect retio

    std::cout<<"Checkpoint : before place"<<std::endl;
    DB.PrintHierNode(current_node);

    
    // Placement
    std::vector<PnRDB::hierNode> nodeVec(numLayout, current_node);
    Placer curr_plc(nodeVec, opath, effort, const_cast<PnRDB::Drc_info&>(drcInfo)); // do placement and update data in current node

    std::cout<<"Checkpoint: generated "<<nodeVec.size()<<" placements\n";
    for(unsigned int lidx=0; lidx<nodeVec.size(); ++lidx) {
      //std::cout<<"Checkpoint: work on layout "<<lidx<<std::endl;
      DB.Extract_RemovePowerPins(nodeVec[lidx]);
      DB.CheckinHierNode(idx, nodeVec[lidx]);
    }
    DB.hierTree[idx].numPlacement = nodeVec.size();

    //TreeVec[idx] = nodeVec;
    //Q.pop();
    if(disable_io)std::cout.clear();
    cout<<"Main-Info: complete node "<<idx<<endl;
  }

  if(disable_io)std::cout.setstate(std::ios_base::failbit);
  int new_topnode_idx = 0;
  auto &currentTree = DB.hierTree[Q.back()];
  for (unsigned int lidx = 0; lidx < currentTree.numPlacement; lidx++) {
    auto &tmp = currentTree.PnRAS[0];
    route_top_down(
        DB, drcInfo,
        PnRDB::bbox(PnRDB::point(0, 0), PnRDB::point(tmp.width, tmp.height)),
        PnRDB::N, Q.back(), new_topnode_idx, lidx, opath, binary_directory, skip_saving_state, adr_mode);
  }
  if(disable_io)std::cout.clear();

  return 0;
}
