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


class TraceAnalyzer:
    _analyzer: Analyzer

    def __init__(
        self,
        analyzer: Analyzer,
        reader: ReaderProtocol,
        output_path: str,
        analysis_param: AnalysisParam,
        analysis_option: AnalysisOption,
    ):
        self._analyzer = Analyzer(reader._reader, output_path, analysis_option, analysis_param)

    def run(self) -> None:
        self._analyzer.run()

    def cleanup(self) -> None:
        self._analyzer.cleanup()
