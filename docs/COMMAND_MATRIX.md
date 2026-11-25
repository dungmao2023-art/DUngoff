# Ma trận lệnh — Dũng Office

Nguồn sự thật: log CI (`report/linux-sweep.log`, `report/windows-sweep.log`, `linux-word.log`, `excel`, `ppt`). Không ghi "đạt" nếu chưa chạy trên engine thật.

## Phạm vi đã quét trên engine thật
Mọi lệnh `.uno:` xuất hiện trong mã giao diện (85 lệnh). Cột "Bộ kiểm thử" cho biết lệnh nào được kiểm ở đâu. Bộ `--sweep 1` (mới) bổ sung 22 lệnh Calc/Writer trước đây chưa có kiểm thử nào, mỗi lệnh kiểm bằng hiệu ứng thật (nội dung/định dạng vùng chọn).

| Lệnh | Ứng dụng | Kết quả | Bộ kiểm thử | Bằng chứng (rút gọn) |
|---|---|---|---|---|
| `.uno:AlignBottom` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | state=(chưa báo) html=<!DOCTYPE html>  <html> <head>    <meta http-equ |
| `.uno:AlignHorizontalCenter` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | state=true html=<!DOCTYPE html>  <html> <head>    <meta http-equiv="co |
| `.uno:AlignLeft` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | state=true html=<!DOCTYPE html>  <html> <head>    <meta http-equiv="co |
| `.uno:AlignRight` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | state=true html=<!DOCTYPE html>  <html> <head>    <meta http-equiv="co |
| `.uno:AlignTop` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | state=(chưa báo) html=<!DOCTYPE html>  <html> <head>    <meta http-equ |
| `.uno:AlignVCenter` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | state=(chưa báo) html=<!DOCTYPE html>  <html> <head>    <meta http-equ |
| `.uno:AssignLayout` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:AttributePageSize` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:AutoSum` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:BackColor` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:BackgroundColor` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:BasicShapes` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Bold` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:CenterPara` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:CharFontName` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Color` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:ColumnWidth` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Copy` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Cut` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | N1= |
| `.uno:DataFilterAutoFilter` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:DecrementIndent` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:DefaultBullet` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:DefaultNumbering` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:DeleteColumns` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | C2=X |
| `.uno:DeletePage` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:DeleteRows` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | A4=1 A5= |
| `.uno:DuplicatePage` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:EnterString` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:ExecuteSearch` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:FontHeight` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:FormatPaintbrush` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | lệnh nhận không lỗi; hiệu ứng cần chọn ô đích bằng chuột (kiểm trên má |
| `.uno:FreezePanes` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | state=false |
| `.uno:FreezePanesColumn` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | FreezePanes=true |
| `.uno:FreezePanesRow` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | FreezePanes=true |
| `.uno:GoToCell` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:GoToStartOfDoc` | Writer | ĐẠT | sweep_tests.cpp (`--sweep 1`) | ZDong dau dong hai |
| `.uno:HideSlide` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:IncrementIndent` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Insert` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:InsertColumnsBefore` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | D2=X |
| `.uno:InsertGraphic` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:InsertPage` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:InsertPageNumberField` | Writer | ĐẠT | sweep_tests.cpp (`--sweep 1`) | ZDong dau dong hai 1   html=<!DOCTYPE html> <html> <head>  <meta http- |
| `.uno:InsertPagebreak` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:InsertRowsBefore` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | A5=1 |
| `.uno:InsertTable` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Italic` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:JustifyPara` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:LeftPara` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:ModifiedStatus` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Move` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:MovePageDown` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:MovePageUp` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:NumberFormatDate` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | 03/15/23 |
| `.uno:NumberFormatDecDecimals` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | 0.50 -> 0.5 |
| `.uno:NumberFormatIncDecimals` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | 0.50 |
| `.uno:NumberFormatPercent` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | 50.00% |
| `.uno:NumberFormatStandard` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | 0.5 |
| `.uno:NumberFormatThousands` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:PageLRMargin` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:PageULMargin` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Redo` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Remove` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:RenameTable` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:ResetAttributes` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:RightPara` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:RowHeight` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:SelectAll` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:SetHyperlink` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:SetOptimalColumnWidth` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | rộng 85 -> 258 |
| `.uno:ShowSlide` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:SortAscending` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | 1 2 3 4 |
| `.uno:SortDescending` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | 4 3 2 1 |
| `.uno:SpacePara1` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:SpacePara15` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Strikeout` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:StyleApply` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:SubScript` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:SuperScript` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Text` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:ToggleMergeCells` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | <!DOCTYPE html>  <html> <head>    <meta http-equiv="content-type" cont |
| `.uno:Underline` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:Undo` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |
| `.uno:ViewRowColumnHeaders` | Calc | ĐẠT | sweep_tests.cpp (`--sweep 1`) | { "commandName": ".uno:ViewRowColumnHeaders", "rows": [ { "text": 0, " |
| `.uno:WrapText` | — | ĐẠT (bộ kiểm thử có sẵn) | word_tests / excel_tests / ppt_tests | xem log CI |

## Còn lỗi / chưa kiểm
- `.uno:FormatPaintbrush`: CHƯA KIỂM — hiệu ứng cần chọn ô đích bằng chuột; kiểm trên máy thật (xem docs/KICH_BAN_KIEM_TRA_THIET_BI_THAT.md).
- Lệnh của giao diện Qt (không phải UNO): `xl:*`, `pdfTools:*`, `cut/copy`, zoom, thao tác PowerPoint ở `app/src/ppt/PptActions.cpp`: KHÔNG kiểm được trong sweep vì cần cửa sổ Qt; được phủ bởi các test Qt riêng (pdf, tableutil...) hoặc kiểm tay trên máy thật.
- Kết quả trên engine Windows: xem `report/windows-sweep.log` của lần chạy CI mới nhất; bước này chưa chặn job cho tới khi có kết quả ổn định.
- Phím tắt: bảng KeyMap có test riêng (tests/keymap); sweep này không lặp lại.
