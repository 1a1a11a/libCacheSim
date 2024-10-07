#!/bin/bash 

dataname=$1
CURR_DIR=$(dirname $(readlink -f $0))

python3 ${CURR_DIR}/access_pattern.py ${dataname}.accessRtime
python3 ${CURR_DIR}/access_pattern.py ${dataname}.accessVtime
python3 ${CURR_DIR}/req_rate.py ${dataname}.reqRate_w300
python3 ${CURR_DIR}/size.py ${dataname}.size
python3 ${CURR_DIR}/reuse.py ${dataname}.reuse
python3 ${CURR_DIR}/popularity.py ${dataname}.popularity
# python3 ${CURR_DIR}/requestAge.py ${dataname}.requestAge
python3 ${CURR_DIR}/size_heatmap.py ${dataname}.sizeWindow_w300
# python3 ${CURR_DIR}/futureReuse.py ${dataname}.access

python3 ${CURR_DIR}/popularity_decay.py ${dataname}.popularityDecay_w300_obj
python3 ${CURR_DIR}/reuse_heatmap.py ${dataname}.reuseWindow_w300
