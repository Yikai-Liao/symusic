import hashlib
from concurrent.futures import ThreadPoolExecutor, as_completed
from os import PathLike
from typing import Optional

import requests


def range_download(url: str, path: PathLike, s_pos: int, e_pos: int):
    headers = {"Range": f"bytes={s_pos}-{e_pos}"}
    res = requests.get(url, headers=headers, stream=True)
    with open(path, "rb+") as f:
        f.seek(s_pos)
        for chunk in res.iter_content(chunk_size=64 * 1024):
            if chunk:
                f.write(chunk)


class Downloader:
    def __init__(self, max_workers=1):
        self.executor = ThreadPoolExecutor(max_workers=max_workers)

    def get(self, url: str, path: PathLike, md5: Optional[str] = None):
        self._create_empty_file(path)
        # 2. calculate the divisional ranges
        divisional_ranges = self._calc_divisional_range(url)
        # 3. download the file
        self._download(url, path, divisional_ranges)
        # 4. check the md5
        if md5 is not None:
            self._check_md5(path, md5)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.executor.shutdown(wait=True)
        return False

    @staticmethod
    def _create_empty_file(path: PathLike):
        with open(path, "wb"):
            pass

    @staticmethod
    def _calc_divisional_range(url: str):
        # get the file size
        res = requests.head(url)
        filesize = int(res.headers["Content-Length"])
        # divide the file into chunks
        step = min(filesize, 5 * 1024 * 1024)  # 5MB
        arr = list(range(0, filesize, step))
        result = []
        for i in range(len(arr) - 1):
            s_pos, e_pos = arr[i], arr[i + 1] - 1
            result.append([s_pos, e_pos])
        result[-1][-1] = filesize - 1
        return result

    def _download(self, url: str, path: PathLike, divisional_ranges):
        futures = []
        for s_pos, e_pos in divisional_ranges:
            futures.append(
                self.executor.submit(range_download, url, path, s_pos, e_pos)
            )
        # wait for all tasks to complete
        as_completed(futures)

    @staticmethod
    def _check_md5(path: PathLike, md5: str):
        with open(path, "rb") as f:
            if hashlib.md5(f.read()).hexdigest() != md5:
                raise ValueError("Downloaded file is broken, with wrong md5")
