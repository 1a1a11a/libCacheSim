"""
This module provides utility functions for working with trace files.
"""

def extract_dataname(datapath: str) -> str:
    """
    Extracts a clean data name from a full trace file path.

    This function takes a path to a trace file and strips the directory
    path and various common suffixes (like .txt, .csv, .zst, .sample10)
    to produce a clean, human-readable name for the trace, suitable for
    use in plot titles and output filenames.

    Args:
        datapath: The full path to the trace data file.

    Returns:
        A cleaned string representing the name of the trace.
    """
    dataname = datapath.split("/")[-1]
    suffixes_to_remove = [
        # File extensions
        ".sample10", ".sample100", ".oracleGeneral", ".bin", ".zst",
        ".csv", ".txt", ".gz",
        # Window suffixes
        "_w300", "_w60", "_obj", "_req",
        # traceAnalyzer output suffixes
        ".reuseWindow", ".sizeWindow", ".popularityDecay", ".popularity",
        ".reqRate", ".reuse", ".size", ".ttl", ".accessPattern",
        ".accessRtime", ".accessVtime", "_reuse",
    ]

    for s in suffixes_to_remove:
        dataname = dataname.replace(s, "")

    return dataname
