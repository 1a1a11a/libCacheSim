"""S3 Bucket data loader with local caching (HuggingFace-style)."""

from __future__ import annotations

import hashlib
import logging
import shutil
from pathlib import Path
from typing import Optional, Union
from urllib.parse import quote

logger = logging.getLogger(__name__)


class DataLoader:
    DEFAULT_BUCKET = "cache-datasets"
    DEFAULT_CACHE_DIR = Path.home() / ".cache/libcachesim_hub"

    def __init__(
        self,
        bucket_name: str = DEFAULT_BUCKET,
        cache_dir: Optional[Union[str, Path]] = None,
        use_auth: bool = False
    ):
        self.bucket_name = bucket_name
        self.cache_dir = Path(cache_dir) if cache_dir else self.DEFAULT_CACHE_DIR
        self.use_auth = use_auth
        self._s3_client = None
        self._ensure_cache_dir()

    def _ensure_cache_dir(self) -> None:
        (self.cache_dir / self.bucket_name).mkdir(parents=True, exist_ok=True)

    @property
    def s3_client(self):
        if self._s3_client is None:
            try:
                import boto3
                from botocore.config import Config
                from botocore import UNSIGNED

                self._s3_client = boto3.client(
                    's3',
                    config=None if self.use_auth else Config(signature_version=UNSIGNED)
                )
            except ImportError:
                raise ImportError("Install boto3: pip install boto3")
        return self._s3_client

    def _cache_path(self, key: str) -> Path:
        safe_name = hashlib.sha256(key.encode()).hexdigest()[:16] + "_" + quote(key, safe='')
        return self.cache_dir / self.bucket_name / safe_name

    def _download(self, key: str, dest: Path) -> None:
        temp = dest.with_suffix(dest.suffix + '.tmp')
        temp.parent.mkdir(parents=True, exist_ok=True)

        try:
            logger.info(f"Downloading s3://{self.bucket_name}/{key}")
            obj = self.s3_client.get_object(Bucket=self.bucket_name, Key=key)
            with open(temp, 'wb') as f:
                f.write(obj['Body'].read())
            shutil.move(str(temp), str(dest))
            logger.info(f"Saved to: {dest}")
        except Exception as e:
            if temp.exists():
                temp.unlink()
            raise RuntimeError(f"Download failed for s3://{self.bucket_name}/{key}: {e}")

    def load(self, key: str, force: bool = False, mode: str = 'rb') -> Union[bytes, str]:
        path = self._cache_path(key)
        if not path.exists() or force:
            self._download(key, path)
        with open(path, mode) as f:
            return f.read()

    def is_cached(self, key: str) -> bool:
        return self._cache_path(key).exists()

    def get_cache_path(self, key: str) -> Path:
        return self._cache_path(key).as_posix()

    def clear_cache(self, key: Optional[str] = None) -> None:
        if key:
            path = self._cache_path(key)
            if path.exists():
                path.unlink()
                logger.info(f"Cleared: {path}")
        else:
            shutil.rmtree(self.cache_dir, ignore_errors=True)
            logger.info(f"Cleared entire cache: {self.cache_dir}")

    def list_cached_files(self) -> list[str]:
        if not self.cache_dir.exists():
            return []
        return [
            str(p) for p in self.cache_dir.rglob('*')
            if p.is_file() and not p.name.endswith('.tmp')
        ]

    def get_cache_size(self) -> int:
        return sum(
            p.stat().st_size for p in self.cache_dir.rglob('*') if p.is_file()
        )

    def list_s3_objects(self, prefix: str = "", delimiter: str = "/") -> dict:
        """
        List S3 objects and pseudo-folders under a prefix.

        Args:
            prefix: The S3 prefix to list under (like folder path)
            delimiter: Use "/" to simulate folder structure

        Returns:
            A dict with two keys:
                - "folders": list of sub-prefixes (folders)
                - "files": list of object keys (files)
        """
        paginator = self.s3_client.get_paginator('list_objects_v2')
        result = {"folders": [], "files": []}

        for page in paginator.paginate(
            Bucket=self.bucket_name,
            Prefix=prefix,
            Delimiter=delimiter
        ):
            # CommonPrefixes are like subdirectories
            result["folders"].extend(cp["Prefix"] for cp in page.get("CommonPrefixes", []))
            result["files"].extend(obj["Key"] for obj in page.get("Contents", []))

        return result
