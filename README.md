name refactoring
dynamic compiling
add tests
add size support


goal: design libMimircache as a library, not a binary
separate LRUProfiler and simulator

rename cacheline
allow a cache to run through a trace so that user can run multiple traces
remove TRACK_ACCESS_TIME in LRUSize
remove cache->core->ts
remove REUSE_DIST and bp from reader (but then how do you pass to python)


How to install libmimircache



How to use libmimircache


Handling storage block cache traces
For the simplicity of most users, libmimircache decides to drop native support for block traces using logical block number/address (LBN/LBA), instead a user is expected to convert the trace into a stardard format,
in which, obj_id is LBA/block_size while obj_size is request_size/block_size. We have provided some tools to help with this conversion, see tools/convert_storage_trace.sh

for efficiency, we stopped supporting using 64-bit obj_id on 32-bit platform