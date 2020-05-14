from PnR import *

rc = toplevel( ["./ex2.py", "./testcase_example", "switched_capacitor_filter.lef", "switched_capacitor_filter.v", "switched_capacitor_filter.map", "layers.json", "switched_capacitor_filter", "2", "0"])

print(f"Return code: {rc}")
