"""Wrapper of Analyzer"""
from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .protocols import ReaderProtocol

from .libcachesim_python import (
    Analyzer,
    AnalysisOption,
    AnalysisParam,
)

# Import ReaderException
class ReaderException(Exception):
    """Exception raised when reader is not compatible"""
    pass

class TraceAnalyzer:
    _analyzer: Analyzer

    def __init__(
        self,
        reader: ReaderProtocol,
        output_path: str,
        analysis_param: AnalysisParam = None,
        analysis_option: AnalysisOption = None,
    ):
        """
        Initialize trace analyzer.

        Args:
            reader: Reader protocol
            output_path: Path to output file
            analysis_param: Analysis parameters
            analysis_option: Analysis options
        """
        if not hasattr(reader, 'c_reader') or not reader.c_reader:
            raise ReaderException("Only C/C++ reader is supported")

        if analysis_param is None:
            analysis_param = AnalysisParam()
        if analysis_option is None:
            analysis_option = AnalysisOption()

        self._analyzer = Analyzer(reader._reader, output_path, analysis_option, analysis_param)

    def run(self) -> None:
        self._analyzer.run()

    def cleanup(self) -> None:
        self._analyzer.cleanup()
