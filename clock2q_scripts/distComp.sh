


rm task 2>/dev/null;
for TRACE in /mnt/cfs/cphy/w*; do 
    OUTPUT=/mnt/cfs/output/$(basename ${TRACE})
    for i in 0.01 0.04 0.08 0.16 0.32 0.48 0.64 0.80 0.90 0.99; do 
        echo "shell:4:2:2:/mnt/cfs/cachesim_data ${TRACE} vscsi clock2q+v2 0.001,0.01,0.1 --ignore-obj-size 1 -e \"corr-window-ratio=${i},fifo-size-ratio=0.1\" | grep result >> ${OUTPUT}" >> task
    done;
    echo "shell:4:2:2:/mnt/cfs/cachesim_data ${TRACE} vscsi S3FIFO 0.001,0.01,0.1 --ignore-obj-size 1 | grep result >> ${OUTPUT}" >> task
    echo "shell:4:2:2:/mnt/cfs/cachesim_data ${TRACE} vscsi S3FIFOv2 0.001,0.01,0.1 --ignore-obj-size 1 | grep result >> ${OUTPUT}" >> task
done

OUTPUT=/tmp/tmp/$(basename ${TRACE})


for f in /mnt/cfs/output/w*; do
    sizes=$(cat ${f} | awk '{print $5}' | sort | uniq)
    for s in ${sizes[@]}; do
        cat ${f} | grep ${s} | sort -nk4,4 -t - >> ${f}.clean
    done
done





for dataset in /mnt/cfs/oracleReuse/*; do
    echo -ne "${dataset}\t"
    for f in ${dataset}/*; do 
        OUTPUT=/mnt/cfs/output/${dataset}/$(basename ${f})
        echo "shell:4:2:2:/mnt/cfs/cachesim_data ${f} oracleGeneral FIFO,S3-FIFO,S3FIFOv2,SIEVE 0.001,0.01,0.1 --ignore-obj-size 1 | grep result >> ${OUTPUT}" >> task
    done
    wc -l
done


