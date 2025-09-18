"""
Provides utility variables and functions specifically for interacting with
the cachesim executable and its outputs.
"""

# A dictionary to map internal, detailed algorithm names from the cachesim
# output to more concise, user-friendly names for plotting and reporting.
algo_name_mapping_dict = {
    "S3FIFO-0.1000-2": "S3-FIFO",
    "WTinyLFU-w0.01-SLRU": "WTinyLFU",
}
