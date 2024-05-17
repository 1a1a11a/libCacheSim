
for f in /disk/cphy_vscsi/w*; do 
    for algo in fifo lru arc lirs twoQ tinylfu sieve; do 
        echo ./bin/cachesim $f vscsi ${algo} 0 --ignore-obj-size 1 >> task; 
    done 
    echo ./bin/cachesim $f vscsi s3fifo,cloud2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2" >> task; 
    for ignore_perc in 0 0.01 0.05 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1; do 
        echo ./bin/cachesim $f vscsi cloud2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2,corr-window-ratio=${ignore_perc}" >> task; 
    done
done

./bin/cachesim $f vscsi cloud2QPlus 0 --ignore-obj-size 1 -e "move-to-main-threshold=2,corr-window-ratio=1"
./bin/cachesim $f vscsi twoQ 0 --ignore-obj-size 1 &
