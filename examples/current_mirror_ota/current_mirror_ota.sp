.subckt current_mirror_ota id vdd vinn vinp vss vout
m17 net16 vinn net24 vss nmos_rvt w=27e-9 l=20e-9 nfin=24
m16 net24 id vss vss nmos_rvt w=27e-9 l=20e-9 nfin=12
m15 net27 vinp net24 vss nmos_rvt w=27e-9 l=20e-9 nfin=24
m14 id id vss vss nmos_rvt w=27e-9 l=20e-9 nfin=12
m11 vbiasnd vbiasnd vss vss nmos_rvt w=27e-9 l=20e-9 nfin=24
m10 vout vbiasnd vss vss nmos_rvt w=27e-9 l=20e-9 nfin=24
m21 net16 net16 vdd vdd pmos_rvt w=27e-9 l=20e-9 nfin=60
m20 vbiasnd net16 vdd vdd pmos_rvt w=27e-9 l=20e-9 nfin=240
m19 net27 net27 vdd vdd pmos_rvt w=27e-9 l=20e-9 nfin=60
m18 vout net27 vdd vdd pmos_rvt w=27e-9 l=20e-9 nfin=240
.ends current_mirror_ota
