#!/bin/bash

docker build -t tally .

docker run --rm --mount source=inputVol,target=/INPUT -it tally bash -c "source sympy/bin/activate && cd /scripts && python placer.py && python global_router.py && cp ota_placer_out.json ota_global_router_out.json /INPUT"

cd ../Cktgen

docker build -t cktgen .

./flow.sh -sv -s cktgen_ota_from_json.py -td ../DetailedRouter/DR_COLLATERAL_Generator/strawman1_ota --show_global_routes