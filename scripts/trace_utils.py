import os


def extract_dataname(tracepath):
    return os.path.basename(tracepath).replace(".oracleGeneral", "").replace(
        ".sample10", "").replace(".bin", "").replace(".zst", "")

