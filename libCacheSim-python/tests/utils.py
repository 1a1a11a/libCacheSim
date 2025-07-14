import os


def get_reference_data(eviction_algo, cache_size_ratio):
    data_file = os.path.join(  # noqa: PTH118
        (os.path.dirname(os.path.dirname(__file__))),  # noqa: PTH120
        "tests",
        "reference.csv"
    )
    with open(data_file, "r") as f:  # noqa: PTH123
        lines = f.readlines()
        key = "3LCache" if eviction_algo == "ThreeLCache" else eviction_algo
        for line in lines:
            if line.startswith(f"{key},{cache_size_ratio}"):
                return float(line.split(",")[-1])
    return None