#!/bin/bash 

dataname=$1

python3 access_pattern.py ${dataname}.access
python3 req_rate.py ${dataname}.reqRate_w300
python3 size.py ${dataname}.size
python3 reuse.py ${dataname}.reuse
python3 popularity.py ${dataname}.popularity
# python3 requestAge.py ${dataname}.requestAge
python3 size_heatmap.py ${dataname}.sizeWindow_w300
# python3 futureReuse.py ${dataname}.access

python3 popularity_decay.py ${dataname}.popularityDecay_w300
python3 reuse_heatmap.py ${dataname}.reuseWindow_w300
