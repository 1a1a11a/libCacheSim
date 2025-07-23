from libcachesim import TraceAnalyzer, TraceReader, DataLoader
import os


def test_analyzer_common():
    # Add debugging and error handling
    loader = DataLoader()
    loader.load("cache_dataset_oracleGeneral/2020_tencentBlock/1K/tencentBlock_1621.oracleGeneral.zst")
    file_path = loader.get_cache_path("cache_dataset_oracleGeneral/2020_tencentBlock/1K/tencentBlock_1621.oracleGeneral.zst")

    reader = TraceReader(file_path)

    analyzer = TraceAnalyzer(reader, output_path="./")

    analyzer.run()
