# SPDX-License-Identifier: MPL-2.0
"""PDF output publication, independent of the PDF engine; accepts an object with save()."""
from __future__ import annotations
import datetime as _dt
import os
import re
import tempfile
import shutil
from dataclasses import dataclass, field
from typing import Iterable, List, Optional

class PdfError(Exception):
    """Vietnamese error for invalid input or unsafe output."""

@dataclass
class SaveResult:
    written: str
    backup: Optional[str] = None
    warnings: List[str] = field(default_factory=list)

def parse_pages(spec: str, total: int) -> List[int]:
    """Chuyển chuỗi kiểu '1-3,5,8-' thành danh sách chỉ số 0-based, có kiểm tra."""
    result: List[int] = []
    if total < 1 or not spec.strip():
        raise PdfError("Chưa chọn trang nào.")
    for part in spec.split(","):
        part = part.strip()
        match = re.fullmatch(r"([0-9]+)\s*-\s*([0-9]*)|-\s*([0-9]+)|([0-9]+)", part)
        if not match:
            raise PdfError(f"Khoảng trang không hợp lệ: {part}")
        first, last, leading_end, single = match.groups()
        try:
            if single is not None:
                start = end = int(single)
            elif leading_end is not None:
                start, end = 1, int(leading_end)
            else:
                start, end = int(first), int(last) if last else total
        except ValueError as exc:
            raise PdfError(f"Khoảng trang không hợp lệ: {part}") from exc
        if start < 1 or end > total or start > end:
            raise PdfError(f"Khoảng trang không hợp lệ: {part} (tài liệu có {total} trang)")
        if len(result) + end - start + 1 > 1_000_000:
            raise PdfError("Vùng chọn vượt 1 triệu trang; hãy chia nhỏ thao tác.")
        result.extend(range(start - 1, end))
    return result



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


def _ensure_distinct_destination(sources: Iterable[str], dest: str) -> None:
    """Reject direct paths, symlinks and hardlinks to any original input."""
    normalized = os.path.normcase(os.path.realpath(dest))
    for source in sources:
        if normalized == os.path.normcase(os.path.realpath(source)):
            raise PdfError("Tệp đích trùng tệp nguồn. Hãy chọn tên mới.")
        try:
            same = os.path.samefile(source, dest)
        except FileNotFoundError:
            same = False
        except OSError as exc:
            raise PdfError(f"Không xác minh được tệp đích: {exc}") from exc
        if same:
            raise PdfError("Tệp đích trùng tệp nguồn. Hãy chọn tên mới.")


def _publish_new(tmp: str, dest: str) -> None:
    """Publish a complete file without overwriting an existing name."""
    try:
        if os.name == "nt":
            os.rename(tmp, dest)
        else:
            os.link(tmp, dest)
    except FileExistsError as exc:
        raise PdfError("Tệp đích vừa được tạo. Hãy chọn tên khác.") from exc
    except OSError as exc:
        raise PdfError(f"Không công bố được bản sao an toàn: {exc}") from exc


def _replace_existing(tmp: str, dest: str) -> None:
    """Final replacement only, after the complete backup is published."""
    os.replace(tmp, dest)


def _same_snapshot(before, after, *, cross_api=False, windows=None) -> bool:
    """Use comparable metadata; Windows path and handle ctime can differ."""
    if windows is None:
        windows = os.name == "nt"
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (
            after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
        return False
    if windows and cross_api:
        # On modern CPython Windows, birthtime is creation time across both APIs.
        # If unavailable, fail closed on differing ctime rather than skip checks.
        first = getattr(before, "st_birthtime_ns", None)
        second = getattr(after, "st_birthtime_ns", None)
        if first is not None and second is not None:
            return first == second
        if first is not None or second is not None:
            return False
    return before.st_ctime_ns == after.st_ctime_ns


def atomic_save(pdf, dest: str, allow_replace: bool = False) -> SaveResult:
    """Ghi `pdf` ra `dest` theo đúng quy tắc an toàn ở trên."""
    dest = os.path.abspath(dest)
    folder = os.path.dirname(dest)
    if not os.path.isdir(folder):
        raise PdfError(f"Thư mục đích không tồn tại: {folder}")

    backup = None
    original_stat = None
    replace_existing = os.path.lexists(dest)
    if replace_existing:
        if not allow_replace:
            raise PdfError("Tệp đích đã tồn tại. Chọn tên khác hoặc xác nhận thay thế.")
        if os.path.islink(dest) or not os.path.isfile(dest):
            raise PdfError("Không thay thế liên kết hoặc đích không phải tệp thường.")
        try:
            original_stat = os.stat(dest, follow_symlinks=False)
            backup = _backup_path(dest)
        except OSError as exc:
            raise PdfError(f"Không xác minh được tệp đích: {exc}") from exc

    tmp = None
    backup_tmp = None
    try:
        fd, tmp = tempfile.mkstemp(prefix="~dung-", suffix=".pdf", dir=folder)
        os.close(fd)
        pdf.save(tmp, linearize=True)
        if not _looks_like_pdf(tmp):
            raise PdfError("Tệp tạm ghi ra không hợp lệ, đã hủy. Tệp gốc không thay đổi.")
        if replace_existing:
            # Exclusive backup creation: never overwrite an existing backup.
            def unchanged(current):
                return _same_snapshot(original_stat, current)
            if not unchanged(os.stat(dest, follow_symlinks=False)):
                raise PdfError("Tệp đích đã thay đổi kể từ lúc xác nhận; không thay thế.")
            with open(dest, "rb") as original:
                handle_stat = os.fstat(original.fileno())
                if not _same_snapshot(original_stat, handle_stat, cross_api=True):
                    raise PdfError("Tệp đích đã thay đổi; không thay thế.")
                backup_fd, backup_tmp = tempfile.mkstemp(prefix="~dung-backup-", suffix=".tmp", dir=folder)
                with os.fdopen(backup_fd, "wb") as saved:
                    shutil.copyfileobj(original, saved)
                    saved.flush()
                    os.fsync(saved.fileno())
                if not _same_snapshot(handle_stat, os.fstat(original.fileno())):
                    raise PdfError("Tệp đích thay đổi trong lúc sao lưu; không thay thế.")
            if not unchanged(os.stat(dest, follow_symlinks=False)):
                raise PdfError("Tệp đích đã thay đổi; không thay thế.")
            # Publish only a complete backup, and never overwrite a competing backup.
            _publish_new(backup_tmp, backup)
            if not unchanged(os.stat(dest, follow_symlinks=False)):
                raise PdfError("Tệp đích đã thay đổi; không thay thế.")
            _replace_existing(tmp, dest)
        else:
            _publish_new(tmp, dest)
    except OSError as exc:
        raise PdfError(f"Không lưu được PDF: {exc}") from exc
    finally:
        for pending in (tmp, backup_tmp):
            if pending and os.path.exists(pending):
                os.remove(pending)
    return SaveResult(written=dest, backup=backup)
