# -*- coding: utf-8 -*-
"""
Created on Wed July 08 13:12:15 2020

@author: kunal
"""

from collections import Counter
from itertools import combinations
from .merge_nodes import merge_nodes
from .write_constraint import compare_two_nodes

import logging
logger = logging.getLogger(__name__)

def create_hierarchy(graph,node,traversed,ports_weight):
    hier_of_node ={}
    level1=list(set(graph.neighbors(node))- set(traversed))
    
    hier_of_node[node]=matching_groups(graph,level1,None)
    logger.debug(f"new hierarchy points {hier_of_node} {level1}")

    if len(hier_of_node[node]) > 0:
        for group in sorted(hier_of_node[node] , key=lambda group: len(hier_of_node[node][group])):
            if len(group)>0:
                templates={}
                similar_node_groups = {}
                for el in sorted(group):
                    similar_node_groups[el]=[el]
                templates[node]=[el]
                visited=group+[node]
                array=similar_node_groups.copy()
                trace_template(graph,similar_node_groups,visited,templates[node],array)
                logger.debug("similar groups final, %s",array)
                
        all_inst = []
        if array and len(array.values())>1 and len(list(array.values())[0])>1:
            matched_ports = {}
            for branch in  array.values():
                for node_hier in branch:
                    if graph.nodes[node_hier]['inst_type'] != 'net' \
                        and node_hier not in all_inst \
                        and not graph.nodes[node_hier]['inst_type'].lower().startswith('cap'):  
                        all_inst.append(node_hier)
                
        else:
            hier_of_node[node]=[]
            for inst in array.keys():
                if graph.nodes[inst]['inst_type']!='net':
                    hier_of_node[node].append(inst)
        if len(all_inst)>1:
            all_inst=sorted(all_inst)
            h_ports_weight={}
            for inst in  all_inst:
                for node_hier in list(set(graph.neighbors(inst))):
                    if graph.nodes[node_hier]['inst_type']== 'net':
                        if (set(graph.neighbors(node_hier))- set(all_inst)):
                            matched_ports[node_hier]=node_hier
                            h_ports_weight[node_hier] = []
                            for nbr in list(graph.neighbors(node_hier)):
                                h_ports_weight[node_hier].append(graph.get_edge_data(node_hier, nbr)['weight'])
                       
            logger.debug(f"creating a new hierarchy for {node}, {all_inst}, {matched_ports}")
            graph, subgraph,_ = merge_nodes(
                    graph, 'dummy_hier_'+node,all_inst , matched_ports)
            hier_of_node[node]={
                        "name": 'dummy_hier_'+node,
                        "graph": subgraph,
                        "ports": list(matched_ports.keys()),
                        "ports_match": matched_ports,
                        "ports_weight": h_ports_weight,
                        "size": len(subgraph.nodes())
                    }
        return hier_of_node

def trace_template(graph, similar_node_groups,visited,template,array):
    next_match={}
    traversed=visited.copy()

    for source,groups in similar_node_groups.items():
        next_match[source]=[]
        for node in groups:
            level1=list(set(graph.neighbors(node))- set(traversed))
            next_match[source] +=level1
            visited +=level1
        if len(next_match[source])==0:
            del next_match[source]

    if len(next_match.keys())> 0 and match_branches(graph,next_match) :
        for source in array.keys():
            if source in next_match.keys():
                array[source]+=next_match[source]
         
        template +=next_match[list(next_match.keys())[0]]
        logger.debug("found matching level: %s,%s,%s",template,similar_node_groups,visited)
        if check_convergence(next_match):
            trace_template(graph, next_match,visited,template,array)

def match_branches(graph,nodes_dict):
    logger.debug(f"matching next level branches {nodes_dict}")
    nbr_values = {}
    for node, nbrs in nodes_dict.items():
        #super_dict={}
        super_list=[]
        if len(nbrs)==1:
            return False
        for nbr in nbrs:
            if graph.nodes[nbr]['inst_type']== 'net':
                super_list.append('net')
                super_list.append(graph.nodes[nbr]['net_type'])
            else:
                super_list.append(graph.nodes[nbr]['inst_type'])
                for v in graph.nodes[nbr]['values'].values():
                    super_list.append(v)
        nbr_values[node]=Counter(super_list)
    _,main=nbr_values.popitem()
    for node, val in nbr_values.items():
        if val == main:
            continue
        else:
            return False
    return True

def FindArray(graph,input_dir,name,ports_weight):
    templates = {}
    array_of_node = {}
    visited =[]
    all_array = {}

    for node, attr in graph.nodes(data=True):
        if  'net' in attr["inst_type"] and len(list(graph.neighbors(node)))>2:
            level1=[l1 for l1 in graph.neighbors(node) if l1 not in visited]
            array_of_node[node]=matching_groups(graph,level1,ports_weight)
            logger.debug("finding array:%s,%s,%s",node,array_of_node[node],level1)
            if len(array_of_node[node]) > 0 and len(array_of_node[node][0])>1:
                for group in array_of_node[node]:
                    similar_node_groups = {}
                    for el in group:
                        similar_node_groups[el]=[el]
                    templates[node]=[el]
                    visited=group+[node]
                    array=similar_node_groups.copy()
                    trace_template(graph,similar_node_groups,visited,templates[node],array)
                    logger.debug("similar groups final, %s",array)
                    all_array[node]=array
    return all_array

def check_convergence(match:dict):
    vals=[]
    for val in match.values():
        if set(val).intersection(vals):
            return False
        else:
            vals+=val

def matching_groups(G,level1,ports_weight):
    similar_groups=[]
    logger.debug("matching groups for all neighbors: %s", level1)
    for l1_node1,l1_node2 in combinations(level1, 2):
        if compare_two_nodes(G,l1_node1,l1_node2,ports_weight):
            found_flag=0
            logger.debug("similar_group %s",similar_groups)
            for index, sublist in enumerate(similar_groups):
                if l1_node1 in sublist and l1_node2 in sublist:
                    found_flag=1
                    break
                if l1_node1 in sublist:
                    similar_groups[index].append(l1_node2)
                    found_flag=1
                    break
                elif l1_node2 in sublist:
                    similar_groups[index].append(l1_node1)
                    found_flag=1
                    break
            if found_flag==0:
                similar_groups.append([l1_node1,l1_node2])
    return similar_groups