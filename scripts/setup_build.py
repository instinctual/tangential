#!/usr/bin/env python3
"""Prepare pinned build tools inside .cache; no system installs or hardware I/O."""

import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tarfile
import urllib.request


ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    if sys.version_info < (3, 12) or platform.system() != "Linux" or platform.machine() != "x86_64":
        raise SystemExit("Setup requires Python 3.12+ on Linux x86_64")
    cache = ROOT / ".cache"
    downloads = cache / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    entries = json.loads((ROOT / "build-lock.json").read_text())["sources"]
    paths = {}
    for entry in entries:
        archive = downloads / entry["archive"]
        if not archive.exists():
            partial = archive.with_suffix(archive.suffix + ".part")
            print("Downloading", entry["archive"], flush=True)
            with urllib.request.urlopen(entry["url"], timeout=60) as response, partial.open("wb") as output:
                while block := response.read(1024 * 1024):
                    output.write(block)
            if sha256(partial) != entry["sha256"]:
                raise RuntimeError("Downloaded archive checksum mismatch")
            partial.replace(archive)
        if sha256(archive) != entry["sha256"]:
            raise RuntimeError("Cached archive checksum mismatch: " + archive.name)
        parent = cache / entry["extract_parent"]
        parent.mkdir(parents=True, exist_ok=True)
        destination = parent / entry["directory"]
        if not destination.exists():
            with tarfile.open(archive) as source:
                source.extractall(parent, filter="data")
        paths[entry["id"]] = destination
    env = os.environ.copy()
    env["PATH"] = str(paths["arm"] / "bin") + ":" + env["PATH"]
    subprocess.run(["make", "TARGETS=stm32/f1", "-j4"], cwd=paths["usb"], env=env, check=True)
    dfu_prefix = cache / "toolchains/dfu-util-0.11"
    if not (dfu_prefix / "bin/dfu-suffix").exists():
        for command in (["./configure", "--prefix=" + str(dfu_prefix)], ["make", "-j4"], ["make", "install"]):
            subprocess.run(command, cwd=paths["dfu"], check=True)
    print("Build tools are ready. Run make test firmware.")


if __name__ == "__main__":
    main()
