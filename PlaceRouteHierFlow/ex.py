from PnRDB.PnRdatabase import PnRdatabase
from PnRDB.datatype import *

p = PnRdatabase( "./testcase_example", "switched_capacitor_filter", "switched_capacitor_filter.v", "switched_capacitor_filter.lef", "switched_capacitor_filter.map", "layers.json")
topo_order = p.TraverseHierTree()
for i in topo_order:
    hN = p.CheckoutHierNode(i)
    print( i, hN.name)
