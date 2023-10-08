import numpy as np
from collections import Counter
from typing import List, Dict, Tuple, Union


def conv_to_cdf(
    data_list: List[Union[int, float]], data_dict: Dict[Union[int, float], int] = None
) -> Tuple[List[float], List[float]]:
    """convert a list of data or a dict of data (count) to cdf

    Args:
        data_list (List[Union[int, float]]): a list of int or float values
        data_dict (Dict[Union[int, float], int], optional): a dict of value count. Defaults to None.

    Returns:
        x, y: the x and y of the cdf
    """
    if data_dict is None and data_list is not None:
        data_dict = Counter(data_list)

    x, y = list(zip(*(sorted(data_dict.items(), key=lambda x: x[0]))))
    y = np.cumsum(y)
    y = y / y[-1]
    return x, y
