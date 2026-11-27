# SPDX-License-Identifier: MPL-2.0
"""PDF output publication, independent of the PDF engine; accepts an object with save()."""
from __future__ import annotations
import datetime as _dt
import os
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
        original_stat = os.stat(dest, follow_symlinks=False)
        backup = _backup_path(dest)

    fd, tmp = tempfile.mkstemp(prefix="~dung-", suffix=".pdf", dir=folder)
    os.close(fd)
    try:
        pdf.save(tmp, linearize=True)
        if not _looks_like_pdf(tmp):
            raise PdfError("Tệp tạm ghi ra không hợp lệ, đã hủy. Tệp gốc không thay đổi.")
        if replace_existing:
            # Exclusive backup creation: never overwrite an existing backup.
            def unchanged(current):
                return (current.st_dev, current.st_ino, current.st_size, current.st_mtime_ns, current.st_ctime_ns) == (
                    original_stat.st_dev, original_stat.st_ino, original_stat.st_size,
                    original_stat.st_mtime_ns, original_stat.st_ctime_ns)
            if not unchanged(os.stat(dest, follow_symlinks=False)):
                raise PdfError("Tệp đích đã thay đổi kể từ lúc xác nhận; không thay thế.")
            with open(dest, "rb") as original:
                if not unchanged(os.fstat(original.fileno())):
                    raise PdfError("Tệp đích đã thay đổi; không thay thế.")
                with open(backup, "xb") as saved:
                    shutil.copyfileobj(original, saved)
                    saved.flush()
                    os.fsync(saved.fileno())
                if not unchanged(os.fstat(original.fileno())):
                    raise PdfError("Tệp đích thay đổi trong lúc sao lưu; không thay thế.")
            if not unchanged(os.stat(dest, follow_symlinks=False)):
                raise PdfError("Tệp đích đã thay đổi; không thay thế.")
            os.replace(tmp, dest)
        else:
            # Windows rename refuses an existing destination; POSIX rename does not.
            # Hardlink publication on POSIX fails closed on unsupported filesystems.
            try:
                if os.name == "nt":
                    os.rename(tmp, dest)
                else:
                    os.link(tmp, dest)
            except FileExistsError as exc:
                raise PdfError("Tệp đích vừa được tạo. Hãy chọn tên khác.") from exc
            except OSError as exc:
                raise PdfError(f"Không công bố được bản sao an toàn: {exc}") from exc
    except OSError as exc:
        raise PdfError(f"Không lưu được PDF: {exc}") from exc
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
    return SaveResult(written=dest, backup=backup)
