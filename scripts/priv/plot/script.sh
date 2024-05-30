
for f in /disk/cphy_vscsi/w*; do 
    for algo in fifo lru arc lirs twoQ tinylfu sieve; do 
        echo ./bin/cachesim $f vscsi ${algo} 0 --ignore-obj-size 1 >> task; 
    done 
    echo ./bin/cachesim $f vscsi s3fifo,Clock2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2" >> task; 
    for ignore_perc in 0 0.01 0.05 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1; do 
        echo ./bin/cachesim $f vscsi Clock2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2,corr-window-ratio=${ignore_perc}" >> task; 
    done
done

./bin/cachesim $f vscsi Clock2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2,corr-window-ratio=1"
./bin/cachesim $f vscsi twoQ 0 --ignore-obj-size 1 &

for f in ../anonymized106/w*; do 
    echo ./cachesim $f vscsi S3FIFOv2 0 --ignore-obj-size 1 -e "move-to-main-threshold=2" >> task
    # echo ./cachesim $f vscsi Clock2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2" >> task
    # echo ./cachesim $f vscsi Clock2QPlus 0 --ignore-obj-size 1 >> task

    for ignore_perc in 0 0.01 0.05 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1; do 
        echo ./cachesim $f vscsi Clock2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2,corr-window-ratio=${ignore_perc}" >> task; 
    done
done

for ignore_perc in 0 0.01 0.05 0.1 0.2 0.4 0.6 0.8 0.9 1; do 
    ./cachesim $f oracleGeneral Clock2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2,corr-window-ratio=${ignore_perc}";
done

f=anonymized106/w100_vscsi1.vscsitrace
./bin/cachesim $f vscsi Clock2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2,corr-window-ratio=1"
./bin/cachesim $f vscsi S3FIFOv2,Clock2QPlus 0 --ignore-obj-size 1 

f=/users/juncheng/data/wiki_2019t.oracleGeneral.sample10.zst
./bin/cachesim $f oracleGeneral S3FIFO,S3FIFOv2,Clock2QPlus 0 --ignore-obj-size 1 &


rm -r bin4/result2; cp -r bin4/result bin4/result2
sed -i "s/Clock2QPlus/Clock2QPlus4/g" bin4/result2/*
rm -rf /disk/result; cp -r result_data /disk/result; for f in bin4/result2/*; do fn=$(basename $f); cat $f >> /disk/result/${fn}; done

rm -r bin_metadata/result2; cp -r bin_metadata/result bin_metadata/result2
sed -i "s/Clock2QPlus/Clock2QPlus4/g" bin_metadata/result2/*
rm -rf /disk/result; cp -r result_metadata /disk/result; for f in bin_metadata/result2/*; do fn=$(basename $f); cat $f >> /disk/result/${fn}; done




for i in `seq 80 106`; do 
    ./bin/cachesim anonymized106/w${i}_vscsi1.vscsitrace vscsi s3fifo,Clock2qplus 0.2,0.4,0.6,0.8,0.9999 --ignore-obj-size 1
done

./bin/cachesim anonymized106/w106_vscsi1.vscsitrace vscsi s3fifo,Clock2qplus 0.2,0.4,0.6,0.8 --ignore-obj-size 1

for f in /disk/data/*; do 
    ./bin/cachesim $f oracleGeneral s3fifo,Clock2qplus 0.01,0.1,0.2,0.4,0.6,0.8,0.9999 --ignore-obj-size 1 &
done


for f in _build/anonymized106/w4*; do 
    python3 scripts/plot_mrc_size.py --tracepath $f --algos fifo,lru,Clock2qplus,s3fifo --trace-format vscsi --sizes 0.001,0.002,0.005,0.01,0.02,0.05,0.1,0.15,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,0.99 --ignore-obj-size
done
python3 scripts/plot_mrc_size.py --tracepath _build/anonymized106/w106_vscsi1.vscsitrace --algos fifo,lru,Clock2qplus,s3fifo --trace-format vscsi --sizes 0.001,0.002,0.005,0.01,0.02,0.05,0.1,0.15,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,0.99 --ignore-obj-size


for f in /disk/data/sample/other/*; do 
    python3 scripts/plot_mrc_size.py --tracepath $f --algos fifo,lru,Clock2qplus,s3fifo --trace-format oracleGeneral --sizes 0.001,0.002,0.005,0.01,0.02,0.05,0.1,0.15,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,0.99 --ignore-obj-size
done

for f in /disk/data/msr/*; do 
    python3 scripts/plot_mrc_size.py --tracepath $f --algos fifo,lru,Clock2qplus,s3fifo --trace-format oracleGeneral --sizes 0.001,0.002,0.005,0.01,0.02,0.05,0.1,0.15,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,0.99 --ignore-obj-size
done


