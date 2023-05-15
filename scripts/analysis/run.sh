dataname=$1

python3 accessPattern.py --datapath=${dataname}.access
python3 reqRate.py --datapath=${dataname}.reqRate_w300
python3 size.py --datapath=${dataname}.size
python3 reuse.py --datapath=${dataname}.reuse
python3 popularity.py --datapath=${dataname}.popularity
python3 requestAge.py --datapath=${dataname}.requestAge
python3 sizeHeatmap.py --datapath=${dataname}.sizeWindow_w300
# python3 futureReuse.py --datapath=${dataname}.access

python3 popularityDecay.py --datapath=${dataname}.popularityDecay_w300_req
python3 popularityDecay.py --datapath=${dataname}.popularityDecay_w300_obj
python3 reuseHeatmap.py --datapath=${dataname}.reuseWindow_w300

