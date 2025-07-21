"""Wrapper misc functions"""
from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .protocols import ReaderProtocol
    from .cache import CacheBase

from .libcachesim_python import convert_to_oracleGeneral, convert_to_lcs, c_process_trace


class Util:
    @staticmethod
    def convert_to_oracleGeneral(reader, ofilepath, output_txt=False, remove_size_change=False):
        return convert_to_oracleGeneral(reader, ofilepath, output_txt, remove_size_change)

    @staticmethod
    def convert_to_lcs(reader, ofilepath, output_txt=False, remove_size_change=False, lcs_ver=1):
        """
        Convert a trace to LCS format.

        Args:
            reader: The reader to convert.
            ofilepath: The path to the output file.
            output_txt: Whether to output the trace in text format.
            remove_size_change: Whether to remove the size change field.
            lcs_ver: The version of LCS format (1, 2, 3, 4, 5, 6, 7, 8).
        """
        return convert_to_lcs(reader, ofilepath, output_txt, remove_size_change, lcs_ver)

    @staticmethod
    def process_trace(cache: CacheBase, reader: ReaderProtocol, start_req: int = 0, max_req: int = -1) -> tuple[float, float]:
        """
        Process a trace with a cache.

        Args:
            cache: The cache to process the trace with.
            reader: The reader to read the trace from.
            start_req: The starting request to process.
            max_req: The maximum number of requests to process.

        Returns:
            tuple[float, float]: The object miss ratio and byte miss ratio.
        """
        # Check if reader is C++ reader
        if not hasattr(reader, 'c_reader') or not reader.c_reader:
            raise ValueError("Reader must be a C++ reader")

        return c_process_trace(cache._cache, reader._reader, start_req, max_req)
