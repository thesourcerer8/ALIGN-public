.subckt cascode_current_mirror_ota id vdd vinn vinp vss vout
m10 id id vss vss nmos_rvt w=27e-9 l=20e-9 nfin=24
m9 net1 id vss vss nmos_rvt w=27e-9 l=20e-9 nfin=24
m1 net2 vinp net1 vss nmos_rvt w=27e-9 l=20e-9 nfin=28
m2 vout vinn net1 vss nmos_rvt w=27e-9 l=20e-9 nfin=28
m5 net2 net2 net3 vdd pmos_rvt w=27e-9 l=20e-9 nfin=60
m6 vout net2 net4 vdd pmos_rvt w=27e-9 l=20e-9 nfin=60
m7 net3 net3 vdd vdd pmos_rvt w=27e-9 l=20e-9 nfin=60
m8 net4 net3 vdd vdd pmos_rvt w=27e-9 l=20e-9 nfin=60
.ends cascode_current_mirror_ota
