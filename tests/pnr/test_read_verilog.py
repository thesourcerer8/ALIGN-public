import json
import pathlib
import io

from align import PnR
from align.pnr.toplevel import ReadVerilogJson, analyze_hN
from align.pnr.build_pnr_model import PnRdatabase, _attach_constraint_files
from align.compiler.write_verilog_lef import write_verilog

mydir = pathlib.Path(__file__).resolve().parent

def test_verilog_input():
    d = mydir / "current_mirror_ota_inputs"

    DB = PnRdatabase( str(d), "current_mirror_ota", "current_mirror_ota.v", "current_mirror_ota.lef", "current_mirror_ota.map", "layers.json")

    for hN in DB.hierTree:
        analyze_hN( "verilog", hN)

def test_verilog_json_input():
    d = mydir / "current_mirror_ota_inputs"

    DB = PnRdatabase( str(d), "current_mirror_ota", "current_mirror_ota.verilog.json", "current_mirror_ota.lef", "current_mirror_ota.map", "layers.json")

    for hN in DB.hierTree:
        analyze_hN( "verilogJson", hN)


def test_diff_verilog_and_verilog_json():
    d = mydir / "current_mirror_ota_inputs"

    with open( d / "current_mirror_ota.verilog.json", "rt") as fp:
        j = json.load( fp)

    with open( d / "current_mirror_ota.v", "rt") as fp:
        vstr = fp.read()

    with io.StringIO() as fp:
        write_verilog( j, fp)
        vvstr = fp.getvalue()
    
    # remove header and trailing spaces
    assert [ line.rstrip(' ') for line in vstr.split('\n')[4:]] == vvstr.split('\n')

    
