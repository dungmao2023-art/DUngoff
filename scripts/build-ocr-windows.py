# SPDX-License-Identifier: MPL-2.0
"""Build a separate pinned Windows OCR runtime. No Office engine rebuild."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import urllib.request

VCPKG = "74e6536215718009aae747d86d84b78376bf9e09"
DATA = "65727574dfcd264acbb0c3e07860e4e9e9b22185"
TESS = "3b7c70e34dea179549ed3e995872e2e019eb8477"
ASSETS = [
    ("tessdata_fast", DATA, "eng.traineddata", "tessdata/eng.traineddata", "bbef4675053b5b468cdb477053e28b1c698ba08e", 4113088),
    ("tessdata_fast", DATA, "vie.traineddata", "tessdata/vie.traineddata", "e54a5912fb634c581c4a45ae9f37cbc021ec7140", 531275),
    ("tessdata_fast", DATA, "LICENSE", "notices/tessdata_fast-LICENSE", "d645695673349e3947e8e5ae42332d0ac3164cd7", 11358),
    ("tesseract", TESS, "tessdata/pdf.ttf", "tessdata/pdf.ttf", "d1472b20ef1aebbf5e11573867e9ac13873681b9", 572),
    ("tesseract", TESS, "tessdata/configs/pdf", "tessdata/configs/pdf", "59645d71ce52a143d819f2057c8c4e9ce2d46e40", 22),
    ("tesseract", TESS, "tessdata/configs/txt", "tessdata/configs/txt", "a0cc952977f0f3562a5c94011c13044ace865519", 166),
    ("tesseract", TESS, "LICENSE", "notices/tesseract-LICENSE", "d645695673349e3947e8e5ae42332d0ac3164cd7", 11358),
]

def run(*args, **kwargs):
    subprocess.run([str(a) for a in args], check=True, **kwargs)

def sha256(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()

def assets(root):
    for repo, rev, name, relative, blob, size in ASSETS:
        url = f"https://raw.githubusercontent.com/tesseract-ocr/{repo}/{rev}/{name}"
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read(size + 1)
        # Pin the upstream Git blob and byte length, not a mutable branch.
        if len(data) != size or hashlib.sha1(f"blob {size}\0".encode() + data).hexdigest() != blob:
            raise RuntimeError(f"Upstream blob mismatch: {name}")
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

def manifest(root):
    return {p.relative_to(root).as_posix(): sha256(p)
            for p in sorted(root.rglob("*")) if p.is_file() and p.name != "verified-source.json"}

def verify(root):
    data = json.loads((root / "verified-source.json").read_text(encoding="utf-8"))
    if data.get("vcpkg_commit") != VCPKG or data.get("tesseract_commit") != TESS or data.get("tessdata_commit") != DATA:
        raise RuntimeError("Unexpected OCR versions")
    if data.get("sha256") != manifest(root):
        raise RuntimeError("OCR bundle contents differ from manifest")
    for repo, rev, name, relative, blob, size in ASSETS:
        content = (root / relative).read_bytes()
        if len(content) != size or hashlib.sha1(f"blob {size}\0".encode() + content).hexdigest() != blob:
            raise RuntimeError(f"OCR asset mismatch: {name}")
    for item in ("tesseract.exe", "notices/tesseract/copyright"):
        if not (root / item).is_file():
            raise RuntimeError(f"Missing OCR component: {item}")
    env = dict(os.environ)
    # No installed Tesseract or vcpkg DLLs on PATH.
    env["PATH"] = os.pathsep.join([str(root), str(Path(os.environ["SystemRoot"]) / "System32")])
    env.pop("TESSDATA_PREFIX", None)
    exe = root / "tesseract.exe"
    result = subprocess.check_output([str(exe), "--version"], env=env, text=True, timeout=30)
    if not result.startswith("tesseract 5.5.1"):
        raise RuntimeError("Unexpected OCR executable version: " + result)
    result = subprocess.check_output([str(exe), "--tessdata-dir", str(root / "tessdata"), "--list-langs"],
                                     env=env, text=True, timeout=30)
    if not {"eng", "vie"} <= set(result.splitlines()):
        raise RuntimeError("OCR languages missing: " + result)
    # Exercise the actual PDF output config, not just --version/--list-langs.
    # A tiny PGM needs no external Python imaging library.
    import tempfile
    with tempfile.TemporaryDirectory(prefix="dung-ocr-probe-") as tmp:
        probe = Path(tmp) / "probe.pgm"
        probe.write_bytes(b"P5\n256 256\n255\n" + bytes([255]) * 65536)
        base = Path(tmp) / "out"
        subprocess.run([str(exe), str(probe), str(base), "--tessdata-dir",
                        str(root / "tessdata"), "-l", "eng", "--psm", "6",
                        "--dpi", "300", "pdf"], env=env, check=True, timeout=30)
        output = base.with_suffix(".pdf")
        if not output.is_file() or not output.read_bytes().startswith(b"%PDF-"):
            raise RuntimeError("OCR runtime cannot generate PDF output")
    print("Verified OCR runtime: " + str(root))

def build(root, work):
    if root.exists() or work.exists():
        raise RuntimeError("Choose new OCR output and build directories")
    work.mkdir(parents=True)
    run("git", "init", work)
    run("git", "-C", work, "fetch", "--depth=1", "https://github.com/microsoft/vcpkg.git", VCPKG)
    run("git", "-C", work, "checkout", "--detach", "FETCH_HEAD")
    if subprocess.check_output(["git", "-C", str(work), "rev-parse", "HEAD"], text=True).strip() != VCPKG:
        raise RuntimeError("vcpkg revision mismatch")
    run("cmd.exe", "/c", str(work / "bootstrap-vcpkg.bat"), "-disableMetrics")
    # Dynamic dependencies retain replaceable DLLs; build release only.
    triplets = work / "dung-triplets"
    triplets.mkdir()
    (triplets / "x64-windows-ocr.cmake").write_text(
        "set(VCPKG_TARGET_ARCHITECTURE x64)\nset(VCPKG_CRT_LINKAGE dynamic)\n"
        "set(VCPKG_LIBRARY_LINKAGE dynamic)\nset(VCPKG_BUILD_TYPE release)\n", encoding="utf-8")
    run(work / "vcpkg.exe", "install", "tesseract:x64-windows-ocr",
        f"--overlay-triplets={triplets}", "--disable-metrics", cwd=work)
    installed = work / "installed" / "x64-windows-ocr"
    root.mkdir(parents=True)
    tool = installed / "tools" / "tesseract"
    shutil.copy2(tool / "tesseract.exe", root / "tesseract.exe")
    for folder in (installed / "bin", tool):
        for dll in folder.glob("*.dll"):
            destination = root / dll.name
            if destination.exists() and sha256(destination) != sha256(dll):
                raise RuntimeError("Conflicting runtime DLL: " + dll.name)
            shutil.copy2(dll, destination)
    for port in (installed / "share").iterdir():
        copyright = port / "copyright"
        if copyright.is_file():
            target = root / "notices" / port.name
            target.mkdir(parents=True, exist_ok=True)
            shutil.copy2(copyright, target / "copyright")
            for file in port.iterdir():
                if file.is_file() and ("license" in file.name.lower() or "notice" in file.name.lower()):
                    shutil.copy2(file, target / file.name)
    assets(root)
    (root / "verified-source.json").write_text(json.dumps({
        "vcpkg_commit": VCPKG, "tesseract_commit": TESS, "tessdata_commit": DATA,
        "sha256": manifest(root),
    }, indent=2, sort_keys=True), encoding="utf-8")
    verify(root)

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("root", type=Path)
    p.add_argument("--work", type=Path)
    p.add_argument("--verify", action="store_true")
    a = p.parse_args()
    if os.name != "nt":
        p.error("Windows runtime build requires Windows")
    if a.verify:
        verify(a.root.resolve())
    elif a.work:
        build(a.root.resolve(), a.work.resolve())
    else:
        p.error("--work is required when building")
