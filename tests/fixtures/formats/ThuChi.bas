Attribute VB_Name = "ThuChi"
Option Explicit
Public Sub ThemGiaoDich()
    Dim s As Worksheet, t As Worksheet, n As Long
    Set s = ThisWorkbook.Worksheets("NhapLieu")
    Set t = ThisWorkbook.Worksheets("GiaoDich")
    If Not IsDate(s.Range("B3").Value) Then GoTo InvalidInput
    If s.Range("B4").Value <> "Thu" And s.Range("B4").Value <> "Chi" Then GoTo InvalidInput
    If Len(Trim$(s.Range("B5").Value)) = 0 Then GoTo InvalidInput
    If Not IsNumeric(s.Range("B6").Value) Then GoTo InvalidInput
    If CDbl(s.Range("B6").Value) <= 0 Then GoTo InvalidInput
    n = t.Cells(t.Rows.Count, 1).End(xlUp).Row + 1
    t.Cells(n, 1).Value = CDate(s.Range("B3").Value)
    t.Cells(n, 1).NumberFormat = "dd/mm/yyyy"
    t.Cells(n, 2).Resize(1, 4).Value = Application.Transpose(s.Range("B4:B7").Value)
    t.Cells(n, 4).NumberFormat = "#,##0"
    XoaNhapLieu
    Application.Calculate
    MsgBox "Da them giao dich.", vbInformation
    Exit Sub
InvalidInput:
    MsgBox "Kiem tra ngay, loai, noi dung va so tien duong.", vbExclamation
End Sub
Public Sub XoaNhapLieu()
    With ThisWorkbook.Worksheets("NhapLieu")
        .Range("B3").Formula = "=TODAY()"
        .Range("B4").Value = "Thu"
        .Range("B5:B7").ClearContents
    End With
End Sub
Public Sub MoBaoCao()
    Application.Calculate
    ThisWorkbook.Worksheets("BaoCao").Activate
End Sub
Public Sub XuatPDF()
    Dim p As Variant
    p = Application.GetSaveAsFilename("BaoCao.pdf", "PDF (*.pdf), *.pdf")
    If VarType(p) = vbBoolean Then Exit Sub
    ThisWorkbook.Worksheets("BaoCao").ExportAsFixedFormat xlTypePDF, CStr(p)
End Sub
