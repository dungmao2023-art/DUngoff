# -*- coding: utf-8 -*-
# Chạy bằng python.exe nội bộ của engine vừa build: kiểm tra (1) pyuno chạy, (2) dung_api đặt lề đúng,
# (3) Python script provider gọi được script theo URL. Kết quả in ra log; thoát 0 nếu đạt hết.
import os, subprocess, sys, time, zipfile, re, tempfile
here = os.path.dirname(os.path.abspath(__file__))
prog = os.environ["DUNG_PROGRAM_DIR"]
sys.path.insert(0, prog)
sys.path.insert(0, here)
import uno
from com.sun.star.beans import PropertyValue

def pv(n, v):
    p = PropertyValue(); p.Name = n; p.Value = v; return p

port = 2002
proc = subprocess.Popen([os.path.join(prog, "soffice.exe"), "--headless", "--norestore", "--nologo",
                         "--accept=socket,host=localhost,port=%d;urp;" % port])
res = {}
try:
    ctx = None
    local = uno.getComponentContext()
    resolver = local.ServiceManager.createInstanceWithContext("com.sun.star.bridge.UnoUrlResolver", local)
    for _ in range(60):
        try:
            ctx = resolver.resolve("uno:socket,host=localhost,port=%d;urp;StarOffice.ComponentContext" % port); break
        except Exception:
            time.sleep(2)
    res["ket_noi_uno"] = ctx is not None
    smgr = ctx.ServiceManager
    desktop = smgr.createInstanceWithContext("com.sun.star.frame.Desktop", ctx)
    doc = desktop.loadComponentFromURL("private:factory/swriter", "_blank", 0, (pv("Hidden", True),))
    doc.Text.setString("Kiem tra le trang")
    import dung_api
    n = dung_api.apply_margins(doc, 30, 15, 20, 20)
    res["so_kieu_trang_doi"] = n
    out = os.path.join(tempfile.gettempdir(), "dung_api_test.docx")
    doc.storeToURL(uno.systemPathToFileUrl(out), (pv("FilterName", "MS Word 2007 XML"),))
    xml = zipfile.ZipFile(out).read("word/document.xml").decode("utf-8", "replace")
    m = re.search(r"<w:pgMar[^>]*>", xml)
    res["pgMar_truc_tiep"] = m.group(0) if m else "KHONG THAY"
    # Python script provider: gọi theo URL như LOK runMacro
    doc2 = desktop.loadComponentFromURL("private:factory/swriter", "_blank", 0, (pv("Hidden", True),))
    try:
        sp = smgr.createInstanceWithContext("com.sun.star.script.provider.MasterScriptProviderFactory", ctx).createScriptProvider(doc2)
        script = sp.getScript("vnd.sun.star.script:dung_api.py$margins_vn?language=Python&location=share")
        r = script.invoke((), (), ())
        res["provider_tra_ve"] = str(r[0])
        out2 = os.path.join(tempfile.gettempdir(), "dung_api_test2.docx")
        doc2.storeToURL(uno.systemPathToFileUrl(out2), (pv("FilterName", "MS Word 2007 XML"),))
        xml2 = zipfile.ZipFile(out2).read("word/document.xml").decode("utf-8", "replace")
        m2 = re.search(r"<w:pgMar[^>]*>", xml2)
        res["pgMar_qua_provider"] = m2.group(0) if m2 else "KHONG THAY"
    except Exception as e:
        res["provider_loi"] = repr(e)
    doc.close(True); doc2.close(True)
    try:
        desktop.terminate()
    except Exception:
        pass
finally:
    time.sleep(2)
    proc.kill()
for k, v in res.items():
    print("DUNG_API %s = %s" % (k, v))
ok = ("1701" in res.get("pgMar_truc_tiep", "")) and ("1701" in res.get("pgMar_qua_provider", ""))
print("DUNG_API KET QUA:", "DAT" if ok else "CHUA DAT")
sys.exit(0 if ok else 1)
