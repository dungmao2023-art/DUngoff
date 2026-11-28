# SPDX-License-Identifier: MPL-2.0
"""Fetch only a fixed immutable PDFium release, verify GitHub's asset digest BEFORE extraction."""
import hashlib, json, pathlib, re, shutil, sys, tarfile, tempfile, urllib.request
TAG = "chromium/8179"
REPO = "bblanchon/pdfium-binaries"
def main():
    platform, path = sys.argv[1:]
    asset = {"linux": "pdfium-linux-x64.tgz", "windows": "pdfium-win-x64.tgz"}[platform]
    root = pathlib.Path(path).resolve()
    if root.exists():
        raise RuntimeError("SDK destination already exists")
    api = "https://api.github.com/repos/" + REPO + "/releases/tags/chromium%2F8179"
    req = urllib.request.Request(api, headers={"Accept":"application/vnd.github+json","User-Agent":"DungOffice-PDFium-SDK","X-GitHub-Api-Version":"2022-11-28"})
    with urllib.request.urlopen(req, timeout=30) as response:
        release = json.load(response)
    if release.get("tag_name") != TAG or not release.get("immutable") or release.get("draft") or release.get("prerelease"):
        raise RuntimeError("Expected immutable upstream release")
    matches = [a for a in release["assets"] if a["name"] == asset]
    if len(matches)!=1:
        raise RuntimeError("Asset missing or ambiguous")
    a = matches[0]
    digest = a.get("digest","")
    url = "https://github.com/" + REPO + "/releases/download/chromium%2F8179/" + asset
    # GitHub may return the slash unescaped in browser_download_url.
    if a["browser_download_url"].replace("/chromium/8179/","/chromium%2F8179/") != url or not re.fullmatch(r"sha256:[0-9a-f]{64}",digest):
        raise RuntimeError("No verified upstream digest")
    with tempfile.TemporaryDirectory() as tmp:
        archive = pathlib.Path(tmp)/asset
        h=hashlib.sha256(); size=0
        with urllib.request.urlopen(url,timeout=90) as response, archive.open("wb") as out:
            while True:
                block=response.read(1024*1024)
                if not block: break
                size+=len(block)
                if size>100*1024*1024: raise RuntimeError("Archive too large")
                h.update(block); out.write(block)
        if "sha256:"+h.hexdigest()!=digest or size!=a["size"]:
            raise RuntimeError("PDFium archive digest or length mismatch")
        print(TAG,asset,digest,flush=True)
        # Never execute downloaded files or extract links/traversal entries.
        root.mkdir(parents=True)
        try:
            with tarfile.open(archive,"r:gz") as tar:
                total=0
                for member in tar:
                    name=pathlib.PurePosixPath(member.name)
                    if name.is_absolute() or ".." in name.parts or "\\" in member.name or ":" in member.name:
                        raise RuntimeError("Unsafe SDK path")
                    if member.isdir(): continue
                    if not member.isfile(): raise RuntimeError("Unsupported SDK archive member")
                    total+=member.size
                    if total>300*1024*1024: raise RuntimeError("SDK too large")
                    dest=root.joinpath(*name.parts)
                    dest.parent.mkdir(parents=True,exist_ok=True)
                    with tar.extractfile(member) as inp,dest.open("xb") as out:
                        shutil.copyfileobj(inp,out)
            required=["include/fpdf_edit.h","include/fpdfview.h"]+(
                ["bin/pdfium.dll","lib/pdfium.dll.lib"] if platform=="windows" else ["lib/libpdfium.so"])
            for name in required:
                if not (root/name).is_file(): raise RuntimeError("Missing SDK member: "+name)
            licenses=[p for p in root.rglob("*") if p.is_file() and (p.name.upper().startswith(("LICENSE","COPYING","NOTICE")) or "licenses" in [x.lower() for x in p.relative_to(root).parts])]
            if not licenses: raise RuntimeError("Missing upstream license files")
            (root/"verified-source.json").write_text(json.dumps({"repository":REPO,"tag":TAG,"asset":asset,"digest":digest,"release_id":release["id"],"asset_id":a["id"],"license_files":[p.relative_to(root).as_posix() for p in licenses]},indent=2),encoding="utf-8")
        except Exception:
            shutil.rmtree(root)
            raise
if __name__=="__main__":
    main()
