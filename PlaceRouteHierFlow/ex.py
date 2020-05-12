from PnRDB.PnRdatabase import *

DB = PnRdatabase( "./testcase_example", "switched_capacitor_filter", "switched_capacitor_filter.v", "switched_capacitor_filter.lef", "switched_capacitor_filter.map", "layers.json")

lefs = DB.checkoutSingleLEF()

topo_order = DB.TraverseHierTree()
for i in topo_order:
    hN = DB.CheckoutHierNode(i)
    print( i, hN.name)
    DB.PrintHierNode( hN)
    DB.WriteDBJSON( hN, f"__json_{hN.name}")
