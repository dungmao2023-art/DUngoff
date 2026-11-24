"""Kiểm tệp DOCX do thí nghiệm cầu nối macro Basic ghi ra: lề, header, thuộc tính tài liệu."""
import sys, zipfile, re
p = sys.argv[1]
z = zipfile.ZipFile(p)
doc = z.read('word/document.xml').decode('utf8')
m = re.search(r'<w:pgMar[^>]*>', doc)
print('pgMar:', m.group(0) if m else 'KHÔNG CÓ')
def twip(a):
    mm = re.search(r'w:%s="(\d+)"' % a, m.group(0)) if m else None
    return round(int(mm.group(1)) * 25.4 / 1440, 1) if mm else None
vals = {k: twip(k) for k in ('left', 'right', 'top', 'bottom')}
print('lề thực tế (mm):', vals)
ok = vals == {'left': 30.0, 'right': 15.0, 'top': 20.0, 'bottom': 20.0}
print('[ĐẠT] ' if ok else '[LỖI] ', 'lề 30/15/20/20 mm')
hdr = [n for n in z.namelist() if n.startswith('word/header')]
hv = any('DUNG-HEADER-THU' in z.read(n).decode('utf8') for n in hdr)
print('[ĐẠT] ' if hv else '[LỖI] ', 'header có chữ DUNG-HEADER-THU', hdr)
core = z.read('docProps/core.xml').decode('utf8') if 'docProps/core.xml' in z.namelist() else ''
tv = 'DUNG-TITLE-THU' in core and 'DUNG-AUTHOR-THU' in core
print('[ĐẠT] ' if tv else '[LỖI] ', 'thuộc tính tài liệu (title/author)')
print('KẾT QUẢ DOCX CẦU NỐI:', 'ĐẠT' if (ok and hv and tv) else 'CÓ LỖI')
