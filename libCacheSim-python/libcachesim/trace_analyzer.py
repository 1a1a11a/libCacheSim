"""Wrapper of Analyzer"""

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .protocols import ReaderProtocol, AnalyzerProtocol

from .libcachesim_python import (
    Analyzer,
    AnalysisOption,
    AnalysisParam,
)


class TraceAnalyzer(AnalyzerProtocol):
    _analyzer: Analyzer

    def __init__(
        self,
        analyzer: Analyzer,
        reader: "ReaderProtocol",
        output_path: str,
        analysis_param: AnalysisParam,
        analysis_option: AnalysisOption,
    ):
        self._analyzer = Analyzer(reader._reader, output_path, analysis_option, analysis_param)

    def run(self) -> None:
        self._analyzer.run()

    def cleanup(self) -> None:
        self._analyzer.cleanup()
