#!/bin/bash
set -e # exit if any command exits with non-zero status

### Tests dataflow of TP links with data from the internal emulator using low level tools ###

# full reset of card
flx-reset ALL; flx-init; flx-reset GTH

echo "enabling readout links:"
sleep 1
for i in $(seq 0 5); do flx-config -d 0 set DECODING_LINK0${i}_EGROUP0_CTRL_EPATH_ENA=1; done;
for i in $(seq 0 5); do flx-config -d 1 set DECODING_LINK0${i}_EGROUP0_CTRL_EPATH_ENA=1; done;

# check elink status
for i in $(seq 0 5); do flx-config -d 0 DECODING_LINK0${i}_EGROUP0_CTRL_EPATH_ENA; done;
for i in $(seq 0 5); do flx-config -d 1 DECODING_LINK0${i}_EGROUP0_CTRL_EPATH_ENA; done;

echo "setting superchunk factor:"
sleep 1
for i in $(seq 0 5); do flx-config -d 0 set SUPER_CHUNK_FACTOR_LINK_0${i}=0x0c; done;
for i in $(seq 0 5); do flx-config -d 1 set SUPER_CHUNK_FACTOR_LINK_0${i}=0x0c; done;

# check superchunk factors have changed
for i in $(seq 0 5); do flx-config -d 0 list | grep SUPER_CHUNK_FACTOR_LINK_0${i}; done;
for i in $(seq 0 5); do flx-config -d 1 list | grep SUPER_CHUNK_FACTOR_LINK_0${i}; done;

sleep 1
EMU_FILE="${DBT_AREA_ROOT}/dtp-patterns/EMUInput/FixedHits_A_emuconfig"
echo "loading pattern: ${EMU_FILE}"
if [ -f "${EMU_FILE}" ]
then
    echo "emulator file found"
    flx-config -d 0 load ${EMU_FILE}
    flx-config -d 1 load ${EMU_FILE}
else
    echo "emulator file not found!"
    exit 1
fi

echo "running fdaq on SLR 0:"
fdaq -d 0 -t 30 > out_fdaq_0.log &
echo "running fdaq on SLR 1:"
fdaq -d 1 -t 30 > out_fdaq_1.log &


# enable hitfinding TPG in GBT mode
hfButler.py flx-0-p2-hf init
hfButler.py flx-1-p2-hf init
hfButler.py flx-0-p2-hf cr-if --on --drop-empty
hfButler.py flx-1-p2-hf cr-if --on --drop-empty
hfButler.py flx-0-p2-hf flowmaster --src-sel gbt --sink-sel hits
hfButler.py flx-1-p2-hf flowmaster --src-sel gbt --sink-sel hits
hfButler.py flx-0-p2-hf link hitfinder -t 20
hfButler.py flx-1-p2-hf link hitfinder -t 20
hfButler.py flx-0-p2-hf link mask enable -c 1-63
hfButler.py flx-1-p2-hf link mask enable -c 1-63
hfButler.py flx-0-p2-hf link config --dr-on --dpr-mux passthrough --drop-empty
hfButler.py flx-1-p2-hf link config --dr-on --dpr-mux passthrough --drop-empty

# enable FELX emulators
femu -d 0 -e
femu -d 1 -e

wait
echo "Test Complete!"
echo "quick summary: "
tail -n 4 out_fdaq_0.log
tail -n 4 out_fdaq_1.log