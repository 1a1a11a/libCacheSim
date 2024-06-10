from typing import List, Dict, Tuple, Optional


def conv_size_str_to_int(size_str: str) -> int:
    """convert the size string to int in bytes (or number of objects)

    Args:
        size_str: the size string, e.g., 1.5GiB, 1024KiB, 1024, 1.5, etc.

    Returns:
        the size in bytes (or number of objects)
    """

    cache_size = 0
    if "KiB" in size_str:
        cache_size = int(float(size_str.strip("KiB")) * 1024)
    elif "MiB" in size_str:
        cache_size = int(float(size_str.strip("MiB")) * 1024 * 1024)
    elif "GiB" in size_str:
        cache_size = int(float(size_str.strip("GiB")) * 1024 * 1024 * 1024)
    elif "TiB" in size_str:
        cache_size = int(float(size_str.strip("TiB")) * 1024 * 1024 * 1024 * 1024)
    else:
        cache_size = int(float(size_str))

    return cache_size


def find_unit_of_cache_size(cache_size: int) -> Tuple[int, str]:
    """convert a cache size in int (byte) to a size with unit"""

    size_unit, size_unit_str = 1, "B"
    if cache_size > 1024 * 1024 * 1024:
        size_unit = 1024 * 1024 * 1024
        size_unit_str = "GiB"
    elif cache_size > 1024 * 1024:
        size_unit = 1024 * 1024
        size_unit_str = "MiB"
    elif cache_size > 1024:
        size_unit = 1024
        size_unit_str = "KiB"

    return size_unit, size_unit_str
