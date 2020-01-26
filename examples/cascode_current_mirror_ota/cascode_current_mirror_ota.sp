.subckt cascode_current_mirror_ota id vdd vinn vinp vss vout
m10 id id vss vss nmos_rvt w=27e-9 l=20e-9 nfin=36
m9 net1 id vss vss nmos_rvt w=27e-9 l=20e-9 nfin=48
m1 net2 vinp net1 vss nmos_rvt w=27e-9 l=20e-9 nfin=192
m2 vout vinn net1 vss nmos_rvt w=27e-9 l=20e-9 nfin=192
m5 net2 net2 net3 vdd pmos_rvt w=27e-9 l=20e-9 nfin=120
m6 vout net2 net4 vdd pmos_rvt w=27e-9 l=20e-9 nfin=120
m7 net3 net3 vdd vdd pmos_rvt w=27e-9 l=20e-9 nfin=120
m8 net4 net3 vdd vdd pmos_rvt w=27e-9 l=20e-9 nfin=120
.ends cascode_current_mirror_ota
