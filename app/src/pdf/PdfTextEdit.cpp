// SPDX-License-Identifier: MPL-2.0
#include "PdfTextEdit.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#ifdef DUNG_HAVE_PDFIUM_EDIT
#include <fpdfview.h>
#include <fpdf_edit.h>
#include <fpdf_text.h>
#include <fpdf_save.h>
#include <fpdf_signature.h>
#include <fpdf_formfill.h>
#endif
namespace pdfedit {
bool available(){
#ifdef DUNG_HAVE_PDFIUM_EDIT
    return true;
#else
    return false;
#endif
}
static dungpdf::Result fail(const QString& error){dungpdf::Result r;r.error=error;return r;}
#ifdef DUNG_HAVE_PDFIUM_EDIT
namespace {
QMutex mutex;
// Keep the same embedded font and exact Unicode mapping. Reject control,
// private-use, unassigned and surrogate characters instead of substituting.
bool editableText(const QString& text){
    if(text.isEmpty()||text.size()>500)return false;
    for(const QChar c:text){
        if(c==QLatin1Char(' '))continue;
        if(!c.isPrint()||c.isSpace()||c.isSurrogate()||
           c.category()==QChar::Other_PrivateUse||c.category()==QChar::Other_NotAssigned)return false;
    }
    return true;
}
struct Session{Session(){FPDF_InitLibrary();}~Session(){FPDF_DestroyLibrary();}};
struct Doc{FPDF_DOCUMENT p;~Doc(){if(p)FPDF_CloseDocument(p);}};
struct Page{FPDF_PAGE p;~Page(){if(p)FPDF_ClosePage(p);}};
struct Text{FPDF_TEXTPAGE p;~Text(){if(p)FPDFText_ClosePage(p);}};
QString readText(FPDF_PAGEOBJECT o,FPDF_TEXTPAGE t){
    auto n=FPDFTextObj_GetText(o,t,nullptr,0);
    if(n<2||n>65536||n%2)return {};
    QVector<FPDF_WCHAR> b; b.resize(int(n/2));
    if(FPDFTextObj_GetText(o,t,b.data(),n)!=n||b.last()!=0)return {};
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(b.constData()),b.size()-1);
}
struct Writer:FPDF_FILEWRITE{
    QFile* f;
    explicit Writer(QFile* file):f(file){
        version=1;
        WriteBlock=[](FPDF_FILEWRITE* self,const void* data,unsigned long size)->int{
            return static_cast<Writer*>(self)->f->write(static_cast<const char*>(data),qint64(size))==qint64(size);
        };
    }
};
QStringList texts(FPDF_PAGE p,FPDF_TEXTPAGE t){
    QStringList result;
    for(int i=0;i<FPDFPage_CountObjects(p);++i){
        auto o=FPDFPage_GetObject(p,i);
        if(o&&FPDFPageObj_GetType(o)==FPDF_PAGEOBJ_TEXT)result<<readText(o,t);
    }
    return result;
}
}
#endif
dungpdf::Result replaceObject(const QString& source,int page,const QString& oldText,const QString& newText,const QString& dest){
#ifndef DUNG_HAVE_PDFIUM_EDIT
    return fail(QStringLiteral("Bản cài chưa có PDFium Editing SDK."));
#else
    if(oldText==newText||!editableText(oldText)||!editableText(newText))
        return fail(QStringLiteral("Chọn một đoạn chữ in được trên một dòng (tối đa 500 ký tự)."));
    const QFileInfo target(dest);
    if(target.exists()||target.isSymLink()||!QDir(target.absolutePath()).exists())return fail(QStringLiteral("Chọn tên tệp mới."));
    QFile input(source);
    if(!input.open(QIODevice::ReadOnly)||input.size()<=0||input.size()>128LL*1024*1024)return fail(QStringLiteral("Nguồn không hợp lệ hoặc vượt 128 MiB."));
    const QByteArray bytes=input.readAll();
    if(input.error()!=QFileDevice::NoError||bytes.size()!=input.size())return fail(QStringLiteral("Không đọc đủ PDF."));
    input.close();
    QMutexLocker lock(&mutex);Session session;
    Doc doc{FPDF_LoadMemDocument64(bytes.constData(),size_t(bytes.size()),nullptr)};
    if(!doc.p)return fail(QStringLiteral("Không mở được PDF."));
    if(FPDF_GetSecurityHandlerRevision(doc.p)!=-1||FPDF_GetSignatureCount(doc.p)!=0||FPDF_GetFormType(doc.p)!=FORMTYPE_NONE)
        return fail(QStringLiteral("Chưa hỗ trợ PDF mã hóa, chữ ký số hoặc form khi sửa chữ."));
    const int pages=FPDF_GetPageCount(doc.p);
    if(page<1||page>pages)return fail(QStringLiteral("Trang không hợp lệ."));
    QStringList expected;
    {
        Page pg{FPDF_LoadPage(doc.p,page-1)};
        if(!pg.p)return fail(QStringLiteral("Không đọc được trang."));
        FPDF_PAGEOBJECT found=nullptr;
        {
            Text tp{FPDFText_LoadPage(pg.p)};
            if(!tp.p)return fail(QStringLiteral("Không đọc được chữ."));
            int count=FPDFPage_CountObjects(pg.p),idx=-1;
            if(count<1||count>50000)return fail(QStringLiteral("Trang quá phức tạp."));
            for(int i=0;i<count;++i){
                auto o=FPDFPage_GetObject(pg.p,i);
                if(!o||FPDFPageObj_GetType(o)!=FPDF_PAGEOBJ_TEXT)continue;
                const auto s=readText(o,tp.p);
                if(s==oldText){
                    if(found||FPDFTextObj_GetTextRenderMode(o)!=FPDF_TEXTRENDERMODE_FILL)
                        return fail(QStringLiteral("Chữ trùng lặp hoặc chế độ vẽ chưa hỗ trợ."));
                    found=o;idx=expected.size();
                }
                expected<<s;
            }
            if(!found)return fail(QStringLiteral("Chọn đúng một đối tượng chữ nguyên vẹn."));
            expected[idx]=newText;
        }
        if(!FPDFText_SetText(found,reinterpret_cast<FPDF_WIDESTRING>(newText.utf16()))||!FPDFPage_GenerateContent(pg.p))
            return fail(QStringLiteral("Font hoặc nội dung không hỗ trợ thay chữ."));
    }
    QTemporaryDir temp(QDir(target.absolutePath()).filePath(QStringLiteral("~dung-edit-XXXXXX")));
    if(!temp.isValid())return fail(QStringLiteral("Không tạo được thư mục tạm."));
    const auto path=temp.filePath(QStringLiteral("edited.pdf"));QFile out(path);
    if(!out.open(QIODevice::WriteOnly))return fail(QStringLiteral("Không ghi được PDF."));
    Writer writer(&out);const bool saved=FPDF_SaveAsCopy(doc.p,&writer,0)&&out.flush();out.close();
    if(!saved||out.error()!=QFileDevice::NoError)return fail(QStringLiteral("Không lưu được PDF."));
    QFile check(path);if(!check.open(QIODevice::ReadOnly))return fail(QStringLiteral("Không đọc lại được PDF."));
    const auto resultBytes=check.readAll();check.close();
    {
        Doc v{FPDF_LoadMemDocument64(resultBytes.constData(),size_t(resultBytes.size()),nullptr)};
        if(!v.p||FPDF_GetPageCount(v.p)!=pages)return fail(QStringLiteral("Bản sửa không đọc lại được."));
        Page p{FPDF_LoadPage(v.p,page-1)};if(!p.p)return fail(QStringLiteral("Không mở lại được trang."));
        Text t{FPDFText_LoadPage(p.p)};
        if(!t.p||texts(p.p,t.p)!=expected)return fail(QStringLiteral("Font gốc không hỗ trợ đủ ký tự mới hoặc chữ sau khi mở lại không khớp. PDF nguồn không thay đổi."));
    }
    const auto info=dungpdf::info(path);
    if(!info.ok||info.pages!=pages||QFileInfo::exists(dest)||QFileInfo(dest).isSymLink()||!QFile::rename(path,dest))
        return fail(QStringLiteral("Không xác minh hoặc công bố được PDF mới."));
    dungpdf::Result r;r.ok=true;r.files<<target.absoluteFilePath();r.warnings<<QStringLiteral("Kiểm tra bố cục khi độ dài chữ thay đổi.");return r;
#endif
}
}
