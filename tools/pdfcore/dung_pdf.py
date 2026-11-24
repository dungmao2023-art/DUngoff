# -*- coding: utf-8 -*-
"""Lõi thao tác PDF của Dũng Office (không phụ thuộc engine LibreOffice).

Quy tắc an toàn (theo docs/DATA_SAFETY.md):
- Không bao giờ ghi đè tệp gốc: luôn ghi ra tệp mới.
- Ghi vào tệp tạm "~dung-xxxx-tên.pdf" cùng thư mục, kiểm tra header %PDF
  và EOF, rồi mới đổi tên nguyên tử sang đích.
- Nếu đích đã tồn tại: từ chối trừ khi allow_replace=True, và khi đó sao lưu
  tệp cũ thành "tên.dung-backup-YYYYMMDD-HHMMSS.pdf" trước khi thay.
- Tệp gốc được đọc ở chế độ chỉ đọc, không bao giờ mở để ghi.
"""
from __future__ import annotations

import datetime as _dt
import os
import secrets
import shutil
from dataclasses import dataclass, field
from typing import Iterable, List, Optional, Sequence

import pikepdf


class PdfError(Exception):
    """Lỗi do người dùng hoặc tệp đầu vào, có thông điệp tiếng Việt."""


@dataclass
class SaveResult:
    written: str
    backup: Optional[str] = None
    warnings: List[str] = field(default_factory=list)


# ---------------------------------------------------------------- an toàn ghi

def _backup_path(path: str) -> str:
    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    root, ext = os.path.splitext(path)
    candidate = f"{root}.dung-backup-{stamp}{ext}"
    n = 1
    while os.path.exists(candidate):
        candidate = f"{root}.dung-backup-{stamp}-{n}{ext}"
        n += 1
    return candidate


def _looks_like_pdf(path: str) -> bool:
    with open(path, "rb") as f:
        head = f.read(8)
        f.seek(0, os.SEEK_END)
        size = f.tell()
        f.seek(max(0, size - 2048))
        tail = f.read()
    return head.startswith(b"%PDF-") and b"%%EOF" in tail


def atomic_save(pdf: pikepdf.Pdf, dest: str, allow_replace: bool = False) -> SaveResult:
    """Ghi `pdf` ra `dest` theo đúng quy tắc an toàn ở trên."""
    dest = os.path.abspath(dest)
    folder = os.path.dirname(dest)
    if not os.path.isdir(folder):
        raise PdfError(f"Thư mục đích không tồn tại: {folder}")

    backup = None
    if os.path.exists(dest):
        if not allow_replace:
            raise PdfError("Tệp đích đã tồn tại. Chọn tên khác hoặc xác nhận thay thế.")
        backup = _backup_path(dest)

    tmp = os.path.join(folder, f"~dung-{secrets.token_hex(4)}-{os.path.basename(dest)}")
    try:
        pdf.save(tmp, linearize=True)
        if not _looks_like_pdf(tmp):
            raise PdfError("Tệp tạm ghi ra không hợp lệ, đã hủy. Tệp gốc không thay đổi.")
        if backup:
            shutil.copy2(dest, backup)
        os.replace(tmp, dest)  # đổi tên nguyên tử trên cùng ổ đĩa
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
    return SaveResult(written=dest, backup=backup)


# ------------------------------------------------------------- thao tác PDF

def open_readonly(path: str, password: Optional[str] = None) -> pikepdf.Pdf:
    try:
        return pikepdf.open(path, password=password or "")
    except pikepdf.PasswordError:
        raise PdfError("Tệp được bảo vệ bằng mật khẩu. Nhập mật khẩu để mở.")
    except pikepdf.PdfError as e:
        raise PdfError(f"Không mở được PDF (có thể hỏng): {e}")


def parse_pages(spec: str, total: int) -> List[int]:
    """Chuyển chuỗi kiểu '1-3,5,8-' thành danh sách chỉ số 0-based, có kiểm tra."""
    result: List[int] = []
    for part in spec.replace(" ", "").split(","):
        if not part:
            continue
        if "-" in part:
            a, _, b = part.partition("-")
            start = int(a) if a else 1
            end = int(b) if b else total
        else:
            start = end = int(part)
        if start < 1 or end > total or start > end:
            raise PdfError(f"Khoảng trang không hợp lệ: {part} (tài liệu có {total} trang)")
        result.extend(range(start - 1, end))
    if not result:
        raise PdfError("Chưa chọn trang nào.")
    return result


def merge(paths: Sequence[str], dest: str, allow_replace: bool = False) -> SaveResult:
    if len(paths) < 2:
        raise PdfError("Cần ít nhất hai tệp để gộp.")
    out = pikepdf.new()
    sources = []
    try:
        for p in paths:
            src = open_readonly(p)
            sources.append(src)
            out.pages.extend(src.pages)
        return atomic_save(out, dest, allow_replace)
    finally:
        out.close()
        for s in sources:
            s.close()


def extract(path: str, pages_spec: str, dest: str, allow_replace: bool = False,
            password: Optional[str] = None) -> SaveResult:
    src = open_readonly(path, password)
    try:
        indexes = parse_pages(pages_spec, len(src.pages))
        out = pikepdf.new()
        for i in indexes:
            out.pages.append(src.pages[i])
        try:
            return atomic_save(out, dest, allow_replace)
        finally:
            out.close()
    finally:
        src.close()


def split_each(path: str, out_dir: str, password: Optional[str] = None) -> List[str]:
    """Tách thành mỗi trang một tệp trong out_dir. Không ghi đè: đích phải chưa tồn tại."""
    src = open_readonly(path, password)
    written: List[str] = []
    try:
        stem = os.path.splitext(os.path.basename(path))[0]
        for i in range(len(src.pages)):
            dest = os.path.join(out_dir, f"{stem}-trang{i + 1:03d}.pdf")
            out = pikepdf.new()
            out.pages.append(src.pages[i])
            try:
                written.append(atomic_save(out, dest).written)
            finally:
                out.close()
    finally:
        src.close()
    return written


def rotate(path: str, pages_spec: str, degrees: int, dest: str,
           allow_replace: bool = False, password: Optional[str] = None) -> SaveResult:
    if degrees % 90 != 0:
        raise PdfError("Chỉ xoay theo bội số của 90 độ.")
    src = open_readonly(path, password)
    try:
        for i in parse_pages(pages_spec, len(src.pages)):
            page = src.pages[i]
            page.rotate(degrees, relative=True)
        return atomic_save(src, dest, allow_replace)
    finally:
        src.close()


def compress(path: str, dest: str, allow_replace: bool = False,
             password: Optional[str] = None) -> SaveResult:
    """Nén bằng cách nén luồng đối tượng và bỏ đối tượng không dùng.

    Không nén lại ảnh (không mất chất lượng). Nếu kết quả lớn hơn bản gốc,
    vẫn ghi nhưng cảnh báo để người dùng biết.
    """
    src = open_readonly(path, password)
    try:
        warnings: List[str] = []
        if src.is_encrypted:
            warnings.append("Tệp có mật khẩu: bản nén sẽ không còn mật khẩu.")
        result = atomic_save(src, dest, allow_replace)
        before = os.path.getsize(path)
        after = os.path.getsize(result.written)
        if after >= before:
            warnings.append("Bản nén không nhỏ hơn bản gốc: tài liệu đã gọn hoặc chủ yếu là ảnh.")
        result.warnings = warnings
        return result
    finally:
        src.close()


def metadata(path: str, password: Optional[str] = None) -> dict:
    src = open_readonly(path, password)
    try:
        info = {}
        with src.open_metadata(set_pikepdf_as_editor=False) as meta:
            for key in ("dc:title", "dc:creator", "pdf:Producer"):
                if key in meta:
                    info[key] = str(meta[key])
        info["pages"] = len(src.pages)
        info["encrypted"] = src.is_encrypted
        return info
    finally:
        src.close()
