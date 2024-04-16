## FAQ
1. **how to read OracleGeneral trace，how to transform from csv to it? **
The [oracleGeneral](/libCacheSim/traceReader/customizedReader/oracle/oracleGeneralBin.h) trace is a binary trace, so you cannot direct read as txt file. Each request uses the following data struct 
```c
struct {
    uint32_t real_time;
    uint64_t obj_id;
    uint32_t obj_size;
    int64_t next_access_vtime;
};
```

* Read the trace: we have provided a tool `tracePrint` that you can use to print the trace in plain text, it is compiled and under `bin/`
* Convert csv to oracleGeneral: we have provided `traceConv` to convert traces. The help menu should be sufficient to get started. 

2. **What are the units in the trace? **
In the [trace.csv](/data/trace.csv), the time unit is in sec, the next_access_time is the logical time (# requests) between current and the next request (to the same object). The next access time is used by some algorithms that require future information, e.g., Belady. The object id is a hash of raw object id (string or numeric value). 
