
def extract_dataname(datapath: str) -> str:
    """
    extract the data name from the datapath

    Args:
        datapath: path to the data file

    Return:
        dataname: the name of the data

    """

    dataname = datapath.split("/")[-1]
    l1 = [
        ".sample10",
        ".sample100",
        ".oracleGeneral",
        ".bin",
        ".zst",
        ".csv",
        ".txt",
        ".gz",
    ]
    l2 = ["_w300", "_w60", "_obj", "_req"]
    l3 = [
        ".reuseWindow",
        ".sizeWindow",
        ".popularityDecay",
        ".popularity",
        ".reqRate",
        ".reuse",
        ".size",
        ".ttl",
        ".accessPattern",
        ".accessRtime",
        ".accessVtime",
        "_reuse",
    ]

    for s in l1 + l2 + l3:
        dataname = dataname.replace(s, "")

    return dataname
