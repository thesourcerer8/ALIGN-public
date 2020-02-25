import json
import argparse
#import gen_gds_json
#import gen_lef
from cell_fabric import Via, Region, DefaultCanvas, Wire, Pdk
from cell_fabric import CenterLineGrid, UncoloredCenterLineGrid
from cell_fabric import EnclosureGrid, SingleGrid, CenteredGrid


class CanvasNMOS(DefaultCanvas):

    def __init__( self, fin_u, fin, finDummy, gate, gateDummy):
        p = Pdk().load('../PDK_Abstraction/FinFET14nm_Mock_PDK/layers.json')
        super().__init__(p)
        assert   3*p['Fin']['Pitch'] < 2*p['M2']['Pitch']

######### Derived Parameters ############
        self.gatesPerUnitCell = gate + 2*gateDummy
        self.finsPerUnitCell = fin + 2*finDummy
        # Must be a multiple of 2
        assert self.finsPerUnitCell % 2 == 0
        assert finDummy % 2 == 0
        assert gateDummy > 0
        # Should be a multiple of 4 for maximum utilization
        assert self.finsPerUnitCell % 4 == 0
        self.m2PerUnitCell = self.finsPerUnitCell//2 + 0
        self.unitCellHeight = self.m2PerUnitCell* p['M2']['Pitch']
        unitCellLength = self.gatesPerUnitCell* p['Poly']['Pitch']
        activeWidth =  p['Fin']['Pitch']*fin
        activeOffset = activeWidth//2 + finDummy* p['Fin']['Pitch']-p['M2']['Pitch']//2+p['Fin']['Offset']
        activePitch = self.unitCellHeight
        RVTPitch = activePitch
        RVTWidth = activeWidth + 2*p['Feol']['active_enclosure']
        RVTOffset = RVTWidth//2 + finDummy* p['Fin']['Pitch']-p['Fin']['Pitch']//2-p['Feol']['active_enclosure']+p['Fin']['Offset']

############Include these all ###########

        self.m0 = self.addGen( Wire( 'm0', 'M0', 'v',
                                     clg=UncoloredCenterLineGrid( pitch=   p['Feol']['m0Pitch'], width= p['Feol']['m0Width'], offset= p['Feol']['m0Pitch']//2),
                                     spg=EnclosureGrid( pitch=activePitch, offset=activeOffset, stoppoint=activeWidth//2, check=True)))

        self.pl = self.addGen( Wire( 'pl', 'poly', 'v',
                                     clg=UncoloredCenterLineGrid( pitch= p['Poly']['Pitch'], width= p['Poly']['Width'], offset= p['Poly']['Offset']),
                                     spg=SingleGrid( offset= p['M2']['Offset'], pitch=self.unitCellHeight)))


        self.fin = self.addGen( Wire( 'fin', 'fin', 'h',
                                      clg=UncoloredCenterLineGrid( pitch= p['Fin']['Pitch'], width= p['Fin']['Width'], offset= p['Fin']['Offset']),
                                      spg=SingleGrid( offset=0, pitch=unitCellLength)))

        stoppoint = (gateDummy-1)* p['Poly']['Pitch'] +  p['Poly']['Offset']

        self.active = self.addGen( Wire( 'active', 'active', 'h',
                                         clg=UncoloredCenterLineGrid( pitch=activePitch, width=activeWidth, offset=activeOffset),
                                         spg=EnclosureGrid( pitch=unitCellLength, offset=0, stoppoint=stoppoint, check=True)))

        self.RVT = self.addGen( Wire( 'RVT', 'polycon', 'h',
                                      clg=UncoloredCenterLineGrid( pitch=RVTPitch, width=RVTWidth, offset=RVTOffset),
                                      spg=EnclosureGrid( pitch=unitCellLength, offset=0, stoppoint=stoppoint, check=True)))

        self.nselect = self.addGen( Region( 'nselect', 'nselect',
                                            v_grid=CenteredGrid( offset= p['Poly']['Pitch']//2, pitch= p['Poly']['Pitch']),
                                            h_grid=self.fin.clg))
        self.pselect = self.addGen( Region( 'pselect', 'pselect',
                                            v_grid=CenteredGrid( offset= p['Poly']['Pitch']//2, pitch= p['Poly']['Pitch']),
                                            h_grid=self.fin.clg))
        self.nwell = self.addGen( Region( 'nwell', 'nwell',
                                            v_grid=CenteredGrid( offset= p['Poly']['Pitch']//2, pitch= p['Poly']['Pitch']),
                                            h_grid=self.fin.clg))

        v0x_offset =  p['M2']['Offset'] + (1+finDummy//2)* p['M2']['Pitch']

        self.v0 = self.addGen( Via( 'v0', 'V0',
                                    h_clg=CenterLineGrid(),
                                    v_clg=self.m1.clg))

        self.v0.h_clg.addCenterLine( 0,                 p['V0']['WidthY'], False)
        for i in range(activeWidth//(2* p['M2']['Pitch'])):
            self.v0.h_clg.addCenterLine( v0x_offset+i*  3*p['Fin']['Pitch'],    p['V0']['WidthY'], True)
        self.v0.h_clg.addCenterLine( self.unitCellHeight,    p['V0']['WidthY'], False)


class UnitCell(CanvasNMOS):

    def unit( self, x, y, x_cells, y_cells, fin_u, fin, finDummy, gate, gateDummy):

        #####   This part generats locations of S/D/G   #####
        gu = self.gatesPerUnitCell
        fin = self.finsPerUnitCell
        h = self.m2PerUnitCell

        SA, SB, DA, DB, GA, GB = ([] for i in range(6))
        for k in range(x_cells//2):
            (p,q) = (gateDummy,gu+gateDummy) if k%2 == 0 else (gu+gateDummy,gateDummy)
            (lSa,lSb) = (2*k*gu+p,2*k*gu+q)
            (lGa,lGb) = (lSa+1,lSb+1)
            (lDa,lDb) = (lSa+gate,lSb+gate)
            SA.append(lSa)
            GA.append(lGa)
            DA.append(lDa)
            SB.append(lSb)
            GB.append(lGb)
            DB.append(lDb)

        (S,D,G) = (SA+SB,DA+DB,GA+GB)

        self.addWire( self.active, 'active', None, y, (x,1), (x+1,-1))
        self.addWire( self.RVT,    'RVT',    None, y, (x, 1), (x+1, -1))

        for i in range(fin):
            self.addWire( self.fin, 'fin', None, fin*y+i, x, x+1)

        #####   Gate Placement   #####
        for i in range(gu):
            self.addWire( self.pl, 'g', None, gu*x+i,   (y,0), (y,1))

        CcM3 = (min(S)+max(S))//2
        Routing = [('SA', SA if y%2==0 else SB, 1, CcM3-1), ('DA', DA if y%2==0 else DB, 2, CcM3-2), ('SB', SB if y%2==0 else SA, 3, CcM3+1), ('DB', DB if y%2==0 else DA, 4, CcM3+2), ('G', G, 5, CcM3)]
        if x_cells-1==x:
            grid_y0 = y*h + finDummy//2-1
            grid_y1 = grid_y0+(fin_u+2)//2
            for i in G:
                self.addWire( self.m1, None, None, i, (grid_y0, -1), (grid_y1, 1))
            for i in S+D:
                SD = 'S' if i in S else 'D'
                self.addWire( self.m1, SD, None, i, (grid_y0, -1), (grid_y1, 1))
                for j in range(1,self.v0.h_clg.n):
                    self.addVia( self.v0, 'v0', None, i, (y, j))

            #pin = 'VDD' if y%2==0 else 'GND'
            #self.addWire( self.m2, pin, pin, h*(y+1), (0, 1), (x_cells*gu, -1))

            for (pin, contact, track, m3route) in Routing:
                self.addWire( self.m2, pin, pin, y*h+track, (min(contact), -1), (max(contact), 1))
                if y_cells > 1:
                   self.addWire( self.m3, None, None, m3route, (track, -1), (y*h+track, 1))
                   self.addVia( self.v2, None, None, m3route, track)
                   self.addVia( self.v2, None, None, m3route, y*h+track)

                for i in contact:
                    self.addVia( self.v1, None, None, i, y*h + track)

        #####   Nselect Placement   #####
        if x == x_cells -1 and y == y_cells -1:
            self.addRegion( self.nselect, 'ns', None, (0, -1), -1, ((1+x)*gu, -1), (y+1)*fin+1)
            #self.addWire( self.m2, 'GND', 'GND', 0, (0, 1), (x_cells*gu, -1))



if __name__ == "__main__":

    parser = argparse.ArgumentParser( description="Inputs for Cell Generation")
    parser.add_argument( "-b", "--block_name", type=str, required=True)
    parser.add_argument( "-n", "--nfin", type=int, required=True)
    parser.add_argument( "-X", "--Xcells", type=int, required=True)
    parser.add_argument( "-Y", "--Ycells", type=int, required=True)
    args = parser.parse_args()
    fin_u = args.nfin
    x_cells = 2*args.Xcells
    y_cells = args.Ycells
    gate = 2
    fin = 2*((fin_u+1)//2)
    gateDummy = 3 ### Total Dummy gates per unit cell: 2*gateDummy
    finDummy = 4  ### Total Dummy fins per unit cell: 2*finDummy

    uc = UnitCell( fin_u, fin, finDummy, gate, gateDummy)

    for (x,y) in ( (x,y) for x in range(x_cells) for y in range(y_cells)):
        uc.unit( x, y, x_cells, y_cells, fin_u, fin, finDummy, gate, gateDummy)

    uc.computeBbox()

    with open(args.block_name + '.json', "wt") as fp:
        data = { 'bbox' : uc.bbox.toList(), 'globalRoutes' : [], 'globalRouteGrid' : [], 'terminals' : uc.terminals}
        fp.write( json.dumps( data, indent=2) + '\n')
    #cell_pin = ["SA", "SB", "G", "DA", "DB"]
    #gen_lef.json_lef(args.block_name + '.json',args.block_name,cell_pin)
    #system('python3 gen_gds_json.py -n %s -j %s.json' % (args.block_name,args.block_name))
    #system('python3 json2gds.py %s.gds.json %s.gds' % (args.block_name,args.block_name))