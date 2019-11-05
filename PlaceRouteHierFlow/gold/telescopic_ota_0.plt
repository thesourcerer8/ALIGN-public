#Use this file as a script for gnuplot
#(See http://www.gnuplot.info/ for details)

set title" #Blocks= 5, #Terminals= 11, #Nets= 16, Area=1.15315e+08, HPWL= 53408 "

set nokey
#   Uncomment these two lines starting with "set"
#   to save an EPS file for inclusion into a latex document
# set terminal postscript eps color solid 20
# set output "result.eps"

#   Uncomment these two lines starting with "set"
#   to save a PS file for printing
# set terminal postscript portrait color solid 20
# set output "result.ps"


set xrange [-10970:10970]

set yrange [-50:10970]

set label "m2_m1" at 5280 , 10080 center 

set label "DA" at 5600 , 9912


set label "DA" at 4960 , 9912


set label "G" at 5280 , 10248


set label "G" at 4640 , 10248


set label "DB" at 5440 , 10080


set label "DB" at 4800 , 10080


set label "S" at 5760 , 9744


set label "S" at 5120 , 9744


set label "m3_m0" at 5280 , 3696 center 

set label "DA" at 5120 , 4200


set label "GA" at 5440 , 3864


set label "S" at 4960 , 4368


set label "DB" at 5280 , 4032


set label "GB" at 5600 , 3696


set label "m5_m4" at 5280 , 1008 center 

set label "DA" at 5600 , 840


set label "DA" at 4960 , 840


set label "S" at 5760 , 672


set label "S" at 5120 , 672


set label "DB" at 5440 , 1008


set label "DB" at 4800 , 1008


set label "GB" at 5280 , 1176


set label "GB" at 4640 , 1176


set label "m6_m7" at 5280 , 8232 center 

set label "DA" at 5280 , 8736


set label "G" at 5280 , 8232


set label "DB" at 5280 , 8400


set label "SA" at 5280 , 8904


set label "SB" at 5280 , 8568


set label "m9_m8" at 5280 , 6384 center 

set label "DA" at 5760 , 5880


set label "G" at 5280 , 6384


set label "DB" at 4480 , 6216


set label "SA" at 6080 , 5712


set label "SB" at 4800 , 6048


set label "vbiasp1" at 5280 , 10920 center 

set label "vdd" at 5760 , 10920 center 

set label "vinn" at 0 , 3864 center 

set label "vinp" at 10560 , 3696 center 

set label "d1" at 5600 , 0 center 

set label "vss" at 5760 , 0 center 

set label "vbiasnd" at 5280 , 0 center 

set label "voutn" at 10560 , 5880 center 

set label "vbiasp2" at 5280 , 10920 center 

set label "voutp" at 0 , 6216 center 

set label "vbiasn" at 5280 , 10920 center 

plot[:][:] '-' with lines linestyle 3, '-' with lines linestyle 7, '-' with lines linestyle 1, '-' with lines linestyle 0

# block m2_m1 select 0 bsize 4
	6560	9240
	6560	10920
	4000	10920
	4000	9240
	6560	9240

# block m3_m0 select 0 bsize 4
	1440	5376
	1440	2016
	9120	2016
	9120	5376
	1440	5376

# block m5_m4 select 0 bsize 4
	10400	168
	10400	1848
	160	1848
	160	168
	10400	168

# block m6_m7 select 0 bsize 4
	2720	9072
	2720	7392
	7840	7392
	7840	9072
	2720	9072

# block m9_m8 select 0 bsize 4
	9120	5544
	9120	7224
	1440	7224
	1440	5544
	9120	5544


EOF
	5640	9504
	5640	10320
	5560	10320
	5560	9504
	5640	9504

	5000	9504
	5000	10320
	4920	10320
	4920	9504
	5000	9504

	5320	9840
	5320	10656
	5240	10656
	5240	9840
	5320	9840

	4680	9840
	4680	10656
	4600	10656
	4600	9840
	4680	9840

	5480	9672
	5480	10488
	5400	10488
	5400	9672
	5480	9672

	4840	9672
	4840	10488
	4760	10488
	4760	9672
	4840	9672

	5800	9336
	5800	10152
	5720	10152
	5720	9336
	5800	9336

	5160	9336
	5160	10152
	5080	10152
	5080	9336
	5160	9336

	5080	5112
	5080	3288
	5160	3288
	5160	5112
	5080	5112

	5400	4776
	5400	2952
	5480	2952
	5480	4776
	5400	4776

	4920	5280
	4920	3456
	5000	3456
	5000	5280
	4920	5280

	5240	4944
	5240	3120
	5320	3120
	5320	4944
	5240	4944

	5560	4608
	5560	2784
	5640	2784
	5640	4608
	5560	4608

	5640	432
	5640	1248
	5560	1248
	5560	432
	5640	432

	5000	432
	5000	1248
	4920	1248
	4920	432
	5000	432

	5800	264
	5800	1080
	5720	1080
	5720	264
	5800	264

	5160	264
	5160	1080
	5080	1080
	5080	264
	5160	264

	5480	600
	5480	1416
	5400	1416
	5400	600
	5480	600

	4840	600
	4840	1416
	4760	1416
	4760	600
	4840	600

	5320	768
	5320	1584
	5240	1584
	5240	768
	5320	768

	4680	768
	4680	1584
	4600	1584
	4600	768
	4680	768

	3448	8768
	3448	8704
	7112	8704
	7112	8768
	3448	8768

	3288	8264
	3288	8200
	7272	8200
	7272	8264
	3288	8264

	4728	8432
	4728	8368
	5832	8368
	5832	8432
	4728	8432

	3128	8936
	3128	8872
	7432	8872
	7432	8936
	3128	8936

	4408	8600
	4408	8536
	6152	8536
	6152	8600
	4408	8600

	8392	5848
	8392	5912
	3128	5912
	3128	5848
	8392	5848

	8552	6352
	8552	6416
	2008	6416
	2008	6352
	8552	6352

	7112	6184
	7112	6248
	1848	6248
	1848	6184
	7112	6184

	8712	5680
	8712	5744
	3448	5744
	3448	5680
	8712	5680

	7432	6016
	7432	6080
	2168	6080
	2168	6016
	7432	6016


EOF

	5260	10900
	5260	10940
	5300	10940
	5300	10900
	5260	10900

	5740	10900
	5740	10940
	5780	10940
	5780	10900
	5740	10900

	-20	3844
	-20	3884
	20	3884
	20	3844
	-20	3844

	10540	3676
	10540	3716
	10580	3716
	10580	3676
	10540	3676

	5580	-20
	5580	20
	5620	20
	5620	-20
	5580	-20

	5740	-20
	5740	20
	5780	20
	5780	-20
	5740	-20

	5260	-20
	5260	20
	5300	20
	5300	-20
	5260	-20

	10540	5860
	10540	5900
	10580	5900
	10580	5860
	10540	5860

	5260	10900
	5260	10940
	5300	10940
	5300	10900
	5260	10900

	-20	6196
	-20	6236
	20	6236
	20	6196
	-20	6196

	5260	10900
	5260	10940
	5300	10940
	5300	10900
	5260	10900

EOF

#Net: net012
	5600	9912
	5280	8568
	5600	9912


#Net: vbiasp1
	5280	10248
	5280	10920
	5280	10248


#Net: net06
	5440	10080
	5280	8904
	5440	10080


#Net: vdd
	5760	9744
	5760	10920
	5760	9744


#Net: net014
	5120	4200
	4800	6048
	5120	4200


#Net: vinn
	5440	3864
	0	3864
	5440	3864


#Net: net10
	4960	4368
	5440	1008
	4960	4368


#Net: net8
	5280	4032
	6080	5712
	5280	4032


#Net: vinp
	5600	3696
	10560	3696
	5600	3696


#Net: d1
	5600	840
	5600	0
	5600	840


#Net: vss
	5760	672
	5760	0
	5760	672


#Net: vbiasnd
	5280	1176
	5280	0
	5280	1176


#Net: voutn
	5280	8736
	5760	5880
	5280	8736

	5280	8736
	10560	5880
	5280	8736


#Net: vbiasp2
	5280	8232
	5280	10920
	5280	8232


#Net: voutp
	5280	8400
	4480	6216
	5280	8400

	5280	8400
	0	6216
	5280	8400


#Net: vbiasn
	5280	6384
	5280	10920
	5280	6384


EOF

pause -1 'Press any key'