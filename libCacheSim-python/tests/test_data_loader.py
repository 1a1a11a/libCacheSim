from libcachesim import DataLoader


def test_data_loader_common():
    loader = DataLoader()
    loader.load("cache_dataset_oracleGeneral/2007_msr/msr_hm_0.oracleGeneral.zst")
    path = loader.get_cache_path("cache_dataset_oracleGeneral/2007_msr/msr_hm_0.oracleGeneral.zst")
    filles = loader.list_s3_objects("cache_dataset_oracleGeneral/2007_msr/")
