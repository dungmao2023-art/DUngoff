# -*- coding: utf-8 -*-
# SPDX-License-Identifier: MPL-2.0
# Test the shipped module and provider in an isolated soffice process.
# This is NOT a test of the live document in LibreOfficeKit.
import os
import pathlib
import subprocess
import sys
import tempfile
import time
import uuid
import xml.etree.ElementTree as ET
import zipfile

prog = pathlib.Path(os.environ["DUNG_PROGRAM_DIR"]).resolve(strict=True)
module_dir = prog.parent / "share" / "Scripts" / "python"
module_path = module_dir / "dung_api.py"
if not module_path.is_file():
    raise RuntimeError("Shipped dung_api.py missing: " + str(module_path))
sys.path.insert(0, str(prog))
sys.path.insert(0, str(module_dir))
import uno
from com.sun.star.beans import PropertyValue
import dung_api

if pathlib.Path(dung_api.__file__).resolve() != module_path.resolve():
    raise RuntimeError("Imported wrong dung_api module")
print("DUNG_API module =", module_path, flush=True)


def pv(n, v):
    p = PropertyValue()
    p.Name, p.Value = n, v
    return p


def assert_margins(doc, path):
    doc.storeToURL(uno.systemPathToFileUrl(str(path)), (pv("FilterName", "MS Word 2007 XML"),))
    ns = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
    with zipfile.ZipFile(path) as z:
        root = ET.fromstring(z.read("word/document.xml"))
    sections = list(root.iter(ns + "pgMar"))
    expected = {"left": 1701, "right": 850, "top": 1134, "bottom": 1134}
    if not sections:
        raise AssertionError("No page margins in output")
    for section in sections:
        for key, val in expected.items():
            actual = int(section.get(ns + key, "-999"))
            if abs(actual - val) > 1:
                raise AssertionError(f"{key}: {actual} != {val}")
    print("DUNG_API verified margins", path.name, flush=True)


def main():
    with tempfile.TemporaryDirectory(prefix="dung-uno-") as temp:
        root = pathlib.Path(temp)
        profile = root / "profile"
        pipe = "dung_" + uuid.uuid4().hex
        endpoint = f"pipe,name={pipe};urp;"
        proc = subprocess.Popen([
            str(prog / "soffice.exe"), "--headless", "--norestore", "--nologo",
            "-env:UserInstallation=" + profile.as_uri(), "--accept=" + endpoint
        ])
        desktop = None
        docs = []
        try:
            local = uno.getComponentContext()
            resolver = local.ServiceManager.createInstanceWithContext("com.sun.star.bridge.UnoUrlResolver", local)
            ctx = None
            for _ in range(60):
                try:
                    ctx = resolver.resolve("uno:" + endpoint + "StarOffice.ComponentContext")
                    break
                except Exception:
                    time.sleep(1)
            if ctx is None:
                raise RuntimeError("Isolated UNO endpoint did not connect")
            smgr = ctx.ServiceManager
            desktop = smgr.createInstanceWithContext("com.sun.star.frame.Desktop", ctx)
            for use_provider in (False, True):
                # Shared Python scripts obtain the active component from the
                # desktop. Hidden documents may not become current even when
                # the provider is created with their model. Use a non-hidden
                # document only in this isolated headless test, and insist on
                # exact component identity before invoking any script.
                doc = desktop.loadComponentFromURL("private:factory/swriter", "_blank", 0, (pv("Hidden", not use_provider),))
                if doc is None:
                    raise RuntimeError("Writer document creation failed")
                docs.append(doc)
                doc.Text.setString("Kiểm tra lề trang")
                if use_provider:
                    if desktop.getCurrentComponent() != doc:
                        raise RuntimeError("Provider target is not the exact active test document; aborting script")
                    factory = smgr.createInstanceWithContext("com.sun.star.script.provider.MasterScriptProviderFactory", ctx)
                    provider = factory.createScriptProvider(doc)
                    script = provider.getScript("vnd.sun.star.script:dung_api.py$margins_vn?language=Python&location=share")
                    result = str(script.invoke((), (), ())[0])
                    if not result.startswith("OK "):
                        raise RuntimeError("Provider failed: " + result)
                else:
                    if dung_api.apply_margins(doc, 30, 15, 20, 20) < 1:
                        raise RuntimeError("No page styles changed")
                assert_margins(doc, root / ("provider.docx" if use_provider else "direct.docx"))
                # Close each synthetic document before opening the next one.
                # This avoids leaving an earlier model current.
                doc.close(True)
                docs.remove(doc)
            print("DUNG_API PASS (standalone UNO, not live LOK)", flush=True)
        finally:
            for doc in reversed(docs):
                try:
                    doc.close(True)
                except Exception:
                    pass
            if desktop is not None:
                try:
                    desktop.terminate()
                except Exception:
                    pass
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=10)


if __name__ == "__main__":
    main()
