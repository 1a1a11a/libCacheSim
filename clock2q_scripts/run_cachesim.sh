#!/bin/bash


TRACE=/mnt/cfs/oracleReuse/cphy/w102.oracleGeneral.bin.zst
# TRACE=/mnt/cfs/oracleReuse/akamai/akamai_lax.ns2.oracleGeneral.bin.zst
# TRACE=/mnt/cfs/oracleReuse/tencentBlock/tencentBlock.ns10000.oracleGeneral.zst
# TRACE=/mnt/cfs/cphy/w105_vscsi1.vscsitrace
# TRACE=$1
OUTPUT=/tmp/tmp/$(basename ${TRACE})

if [[ ${TRACE} == *"oracleGeneral"* ]]; then 
    TRACE_TYPE="oracleGeneral";
elif [[ ${TRACE} == *"vscsi"* ]]; then
    TRACE_TYPE="vscsi";
fi
echo "TRACE_TYPE: ${TRACE_TYPE}"

rm ${OUTPUT} 2>/dev/null;
# for i in 0.01 0.02 0.04 0.08 0.16 0.32 0.48 0.64 0.80 0.95 0.99; do 
for i in 0.01 0.04 0.08 0.16 0.32 0.48 0.64 0.80; do 
    ./bin/cachesim ${TRACE} ${TRACE_TYPE} clock2q+v2 0.01,0.1 --ignore-obj-size 1 \
    -e "corr-window-ratio=${i},fifo-size-ratio=0.1" | grep result >> ${OUTPUT} & 
    # ./bin/cachesim ${TRACE} ${TRACE_TYPE} clock2q+v2 0.01,0.1 --ignore-obj-size 1 \
    # -e "corr-window-ratio=${i},fifo-size-ratio=0.1,move-to-main-threshold=1" | grep result >> ${OUTPUT} & 
done; 
./bin/cachesim ${TRACE} ${TRACE_TYPE} S3FIFO 0.01,0.1 --ignore-obj-size 1 | grep result >> ${OUTPUT} & 
./bin/cachesim ${TRACE} ${TRACE_TYPE} S3FIFOv2 0.01,0.1 --ignore-obj-size 1 | grep result >> ${OUTPUT} & 
wait


sizes=$(cat ${OUTPUT} | awk '{print $5}' | sort | uniq)
for s in ${sizes[@]}; do
    cat ${OUTPUT} | grep ${s} | sort -nk4,4 -t - > ${OUTPUT}
done
