// SPDX-License-Identifier: MPL-2.0
// Ánh xạ mã lệnh (ribbon, thanh bên) -> tên biểu tượng. Một chỗ duy nhất để đổi hình của một nút.
#include "Icons.h"

#include <QHash>

namespace dung::ui {

QString iconNameForCommand(const QString& id) {
    static const QHash<QString, QString> m = {
        // Tệp / ứng dụng
        {"save", "save"}, {"saveAs", "save-as"}, {"exportPdf", "pdf"}, {"print", "print"}, {"open", "open"}, {"close", "close"},
        {"zoomIn", "zoom-in"}, {"zoomOut", "zoom-out"}, {"zoom100", "zoom-reset"},
        // Bộ nhớ tạm / hoàn tác
        {"paste", "paste"}, {"cut", "cut"}, {"copy", "copy"},
        {".uno:SelectAll", "select-all"}, {".uno:FormatPaintbrush", "format-painter"},
        {".uno:Undo", "undo"}, {".uno:Redo", "redo"},
        // Phông chữ / đoạn
        {".uno:Bold", "bold"}, {".uno:Italic", "italic"}, {".uno:Underline", "underline"}, {".uno:Strikeout", "strikethrough"},
        {".uno:SubScript", "subscript"}, {".uno:SuperScript", "superscript"}, {".uno:ResetAttributes", "clear-format"},
        {".uno:LeftPara", "align-left"}, {".uno:CenterPara", "align-center"}, {".uno:RightPara", "align-right"}, {".uno:JustifyPara", "justify"},
        {".uno:DefaultBullet", "bullets"}, {".uno:DefaultNumbering", "numbering"},
        {".uno:IncrementIndent", "indent"}, {".uno:DecrementIndent", "outdent"},
        {".uno:SpacePara1", "line-spacing"}, {".uno:SpacePara15", "line-spacing"},
        {".uno:InsertPagebreak", "page-break"}, {".uno:InsertPageNumberField", "page-number"},
        // Word
        {"word:style0", "text"}, {"word:style1", "heading"}, {"word:style2", "heading"}, {"word:style3", "heading"},
        {"word:font", "font-name"}, {"word:size", "font-size"}, {"word:color", "font-color"},
        {"word:find", "search"}, {"word:replace", "replace"}, {"word:table", "table"}, {"word:image", "image"}, {"word:link", "link"},
        {"word:a4", "paper-size"}, {"word:landscape", "orientation"},
        {"word:marginNarrow", "margins"}, {"word:marginCustom", "margins"}, {"word:marginNormal", "margins"}, {"word:marginVN", "margins"},
        {"word:adminHead", "heading"}, {"word:adminTail", "text"}, {"word:adminFull", "template"}, {"word:adminMargin", "margins"},
        {"word:count", "word-count"}, {"word:back", "back"},
        {"word:reviewCount", "word-count"}, {"word:reviewSyllable", "spell"},
        {".uno:TrackChanges", "rename"}, {".uno:ShowTrackedChanges", "eye"},
        {".uno:PreviousTrackedChange", "move-up"}, {".uno:NextTrackedChange", "move-down"},
        {".uno:AcceptTrackedChange", "info"}, {".uno:RejectTrackedChange", "close"}, {"word:acceptAllReview", "info"}, {"word:rejectAllReview", "close"}, {"word:addComment", "text"},
        {"word:inspect", "search"}, {"word:outline", "heading"}, {"word:symbols", "text"},
        {"word:headerEnable", "heading"}, {"word:footerEnable", "page-number"},
        {"word:footnote", "text"}, {"word:endnote", "text"}, {".uno:UpdateAllIndexes", "toc"},
        {"word:mailMerge", "mail-merge"}, {"word:print", "print"}, {"word:exportPdf", "pdf"},
        {".uno:SpacePara2", "line-spacing"}, {".uno:GoToStartOfDoc", "move-up"}, {".uno:GoToEndOfDoc", "move-down"},
        {"word:vnDate", "calendar"}, {"word:vnMoney", "currency"}, {"word:vnSyllable", "spell"}, {"word:vnTcvn3", "transfer"},
        {".uno:ChangeCaseToUpper", "font-name"}, {".uno:ChangeCaseToLower", "font-name"}, {".uno:ChangeCaseToSentenceCase", "font-name"},
        {".uno:ChangeCaseToTitleCase", "font-name"}, {".uno:ChangeCaseToToggleCase", "font-name"}, {".uno:InsertSymbol", "text"},
        // Excel
        {".uno:FillDown", "move-down"}, {".uno:FillRight", "transfer"},
        {"xl:fillDown", "move-down"}, {"xl:fillRight", "transfer"},
        {"xl:fillUp", "move-up"}, {"xl:fillLeft", "transfer"}, {"xl:fillSeries", "sum"},
        {".uno:AlignLeft", "align-left"}, {".uno:AlignHorizontalCenter", "align-center"}, {".uno:AlignRight", "align-right"},
        {".uno:AlignTop", "align-top"}, {".uno:AlignVCenter", "align-middle"}, {".uno:AlignBottom", "align-bottom"},
        {".uno:AutoSum", "sum"}, {".uno:DataFilterAutoFilter", "filter"},
        {".uno:DeleteColumns", "col-delete"}, {".uno:DeleteRows", "row-delete"},
        {"xl:deleteRow", "row-delete"}, {"xl:deleteColumn", "col-delete"},
        {".uno:InsertColumnsBefore", "col-insert"}, {".uno:InsertRowsBefore", "row-insert"},
        {".uno:InsertColumnsAfter", "col-insert"}, {".uno:InsertRowsAfter", "row-insert"},
        {"word:tableDeleteRow", "row-delete"}, {"word:tableDeleteColumn", "col-delete"}, {"word:tableMergeRow", "merge-cells"},
        {".uno:FreezePanes", "freeze"}, {".uno:FreezePanesColumn", "freeze"}, {".uno:FreezePanesRow", "freeze"},
        {".uno:NumberFormatDate", "calendar"}, {".uno:NumberFormatDecDecimals", "decimal-dec"}, {".uno:NumberFormatIncDecimals", "decimal-inc"},
        {".uno:NumberFormatPercent", "percent"}, {".uno:NumberFormatStandard", "clear-format"}, {".uno:NumberFormatThousands", "thousands"},
        {".uno:SetOptimalColumnWidth", "col-width"}, {".uno:SortAscending", "sort"}, {".uno:SortDescending", "sort"},
        {"xl:sortAscending", "sort"}, {"xl:sortDescending", "sort"},
        {".uno:ToggleMergeCells", "merge-cells"}, {".uno:WrapText", "wrap-text"},
        {"xl:colWidth", "col-width"}, {"xl:rowHeight", "row-height"}, {"xl:fillColor", "fill-color"}, {"xl:textColor", "font-color"},
        {".uno:RecalcPivotTable", "transfer"}, {"xl:refreshPivot", "transfer"}, {"xl:insertChart", "col-width"}, {"xl:formulaAssistant", "sum"}, {"xl:rangeTools", "col-width"}, {"xl:vnMoney", "currency"},
        {"xl:find", "search"}, {"xl:replace", "replace"}, {"xl:fontName", "font-name"}, {"xl:fontSize", "font-size"},
        {"xl:sheetAdd", "add"}, {"xl:sheetDelete", "delete"}, {"xl:sheetDup", "slide-duplicate"}, {"xl:sheetRename", "rename"},
        {"xl:sheetMoveLeft", "transfer"}, {"xl:sheetMoveRight", "transfer"},
        {"tableTools:clean", "clear-format"}, {"tableTools:compare", "compare"}, {"tableTools:dedup", "duplicate-files"},
        // PowerPoint
        {".uno:AssignLayout", "template"}, {".uno:FontHeight", "font-size"}, {".uno:Color", "font-color"}, {".uno:InsertTable", "table"},
        {".uno:InsertPage", "slide-new"}, {".uno:DuplicatePage", "slide-duplicate"}, {".uno:DeletePage", "delete"}, {"ppt.deleteSlide", "delete"},
        {".uno:MovePageUp", "move-up"}, {".uno:MovePageDown", "move-down"},
        {".uno:HideSlide", "eye-off"}, {".uno:ShowSlide", "eye"},
        {"ppt.image", "image"}, {"ppt.rect", "shape"}, {"ppt.textbox", "textbox"},
        {"ppt.notes", "text"}, {"ppt.normal", "template"}, {"ppt.goto", "page-number"}, {"ppt.fontSize", "font-size"}, {"ppt.textColor", "font-color"},
        {"ppt.show", "play"}, {"ppt.showHere", "play"}, {"ppt.transitionNote", "transition"},
        {"ppt.preview", "play"}, {"ppt.showTimed", "play"}, {"ppt.animationCopy", "transition"}, {"ppt.clearAnimationCopy", "delete"}, {"ppt.masterBackground", "template"},
        // PDF
        {"pdfPrev", "move-up"}, {"pdfNext", "move-down"}, {"pdfEditCopy", "text"},
        {".uno:Text", "textbox"}, {".uno:BasicShapes.rectangle", "shape"},
        {"pdfTools:merge", "merge-pdf"}, {"pdfTools:split", "split"}, {"pdfTools:rotate", "rotate"}, {"pdfTools:compress", "compress"},
        {"pdfTools:images", "image"}, {"pdfTools:blank", "new"}, {"pdfTools:number", "page-number"}, {"pdfTools:stamp", "stamp"},
        {"pdfTools:info", "info"}, {"pdfTools:reorder", "move-up"}, {"pdfTools:ocr", "text"}, {"pdfTools:search", "search"},
    };
    // Lệnh có tham số: ".uno:Color#{...}" -> tra theo phần trước dấu #.
    const int hash = id.indexOf(QLatin1Char('#'));
    return m.value(hash > 0 ? id.left(hash) : id);
}

} // namespace dung::ui
