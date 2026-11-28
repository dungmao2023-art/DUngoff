// SPDX-License-Identifier: MPL-2.0
#include "PdfTextEdit.h"
#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <QImageReader>
#include <QImage>
#include <cmath>
#include <cstring>
#include <optional>
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
static dungpdf::Result editImage(const QString& source,int page,const QString& imagePath,
                           std::optional<QPointF> selected,QPointF center,
                           double widthPercent,const QByteArray& expectedSource,const QString& dest){
#ifndef DUNG_HAVE_PDFIUM_EDIT
    return fail(QStringLiteral("Bản cài chưa có PDFium Editing SDK."));
#else
    if(expectedSource.size()!=32||!std::isfinite(center.x())||!std::isfinite(center.y())||
       center.x()<0||center.x()>1||center.y()<0||center.y()>1||
       !std::isfinite(widthPercent)||widthPercent>100||(widthPercent<5&&!(selected&&widthPercent==0)))
        return fail(QStringLiteral("Vị trí hoặc kích thước ảnh không hợp lệ."));
    const bool moving=selected.has_value();
    const bool replacing=moving&&!imagePath.isEmpty();
    if(moving&&(!std::isfinite(selected->x())||!std::isfinite(selected->y())||
        selected->x()<0||selected->x()>1||selected->y()<0||selected->y()>1))
        return fail(QStringLiteral("Vùng chọn ảnh không hợp lệ."));
    const QFileInfo target(dest),imgInfo(imagePath);
    if(target.exists()||target.isSymLink()||!QDir(target.absolutePath()).exists())
        return fail(QStringLiteral("Chọn tên tệp mới."));
    QImage image;
    if(!moving||replacing){
        if(!imgInfo.isFile()||imgInfo.size()<=0||imgInfo.size()>32LL*1024*1024)
            return fail(QStringLiteral("Ảnh không hợp lệ hoặc vượt 32 MiB."));
        QImageReader reader(imagePath);reader.setAutoTransform(true);
        const QSize declared=reader.size();
        if(declared.isEmpty()||qint64(declared.width())*declared.height()>16000000)
            return fail(QStringLiteral("Ảnh vượt 16 triệu điểm ảnh hoặc không đọc được."));
        image=reader.read().convertToFormat(QImage::Format_ARGB32);
        if(image.isNull()||qint64(image.width())*image.height()>16000000)
            return fail(QStringLiteral("Không đọc được ảnh."));
    }
    QFile input(source);
    if(!input.open(QIODevice::ReadOnly)||input.size()<=0||input.size()>128LL*1024*1024)
        return fail(QStringLiteral("Nguồn không hợp lệ hoặc vượt 128 MiB."));
    const auto bytes=input.readAll();
    const bool readOk=input.error()==QFileDevice::NoError&&bytes.size()==input.size();
    input.close();
    if(!readOk||QCryptographicHash::hash(bytes,QCryptographicHash::Sha256)!=expectedSource)
        return fail(QStringLiteral("PDF nguồn đã thay đổi."));
    QMutexLocker lock(&mutex);Session session;
    Doc doc{FPDF_LoadMemDocument64(bytes.constData(),size_t(bytes.size()),nullptr)};
    if(!doc.p||FPDF_GetSecurityHandlerRevision(doc.p)!=-1||FPDF_GetSignatureCount(doc.p)!=0||
       FPDF_GetFormType(doc.p)!=FORMTYPE_NONE)
        return fail(QStringLiteral("Chưa hỗ trợ PDF mã hóa, chữ ký số hoặc form khi chèn ảnh."));
    const int pages=FPDF_GetPageCount(doc.p);
    if(page<1||page>pages)return fail(QStringLiteral("Trang không hợp lệ."));
    QStringList originalText;int originalCount=0,originalImages=0,selectedIndex=-1;double x=0,y=0,w=0,h=0;
    {
        Page pg{FPDF_LoadPage(doc.p,page-1)};
        if(!pg.p)return fail(QStringLiteral("Không mở được trang."));
        const double pw=FPDF_GetPageWidthF(pg.p),ph=FPDF_GetPageHeightF(pg.p);
        FS_RECTF box{};
        if(FPDFPage_GetRotation(pg.p)!=0||!FPDF_GetPageBoundingBox(pg.p,&box)||
           !std::isfinite(pw)||!std::isfinite(ph)||pw<=0||ph<=0||
           std::abs(box.left)>.01||std::abs(box.bottom)>.01||
           std::abs(box.right-pw)>.01||std::abs(box.top-ph)>.01)
            return fail(QStringLiteral("Chưa hỗ trợ trang xoay hoặc trang có gốc tọa độ khác 0."));
        originalCount=FPDFPage_CountObjects(pg.p);
        if(originalCount<0||originalCount>50000)return fail(QStringLiteral("Trang quá phức tạp."));
        {Text text{FPDFText_LoadPage(pg.p)};if(!text.p)return fail(QStringLiteral("Không đọc được lớp chữ."));originalText=texts(pg.p,text.p);}
        FPDF_PAGEOBJECT object=nullptr;
        for(int i=0;i<originalCount;++i){
            auto candidate=FPDFPage_GetObject(pg.p,i);
            if(!candidate||FPDFPageObj_GetType(candidate)!=FPDF_PAGEOBJ_IMAGE)continue;
            ++originalImages;
            if(!moving)continue;
            float l=0,b=0,r=0,t=0;
            if(!FPDFPageObj_GetBounds(candidate,&l,&b,&r,&t))continue;
            const double sx=selected->x()*pw,sy=(1-selected->y())*ph;
            if(sx>=l&&sx<=r&&sy>=b&&sy<=t){
                if(object)return fail(QStringLiteral("Có nhiều ảnh chồng nhau tại vị trí chọn."));
                object=candidate;selectedIndex=i;
            }
        }
        if(moving){
            if(!object)return fail(QStringLiteral("Không tìm thấy ảnh độc lập ở vị trí chọn."));
            FS_MATRIX matrix{};
            if(!FPDFPageObj_GetMatrix(object,&matrix)||!std::isfinite(matrix.a)||!std::isfinite(matrix.d)||
                matrix.a<=0||matrix.d<=0||std::abs(matrix.b)>.0001||std::abs(matrix.c)>.0001)
                return fail(QStringLiteral("Chưa hỗ trợ chỉnh ảnh xoay, nghiêng hoặc lật."));
            if(replacing){
                // Fit the new image inside the original rectangle, retaining
                // its center, and create a separate image resource below.
                const double fit=qMin(double(matrix.a)/image.width(),double(matrix.d)/image.height());
                w=image.width()*fit;h=image.height()*fit;
                center=QPointF((matrix.e+matrix.a/2.0)/pw,1-(matrix.f+matrix.d/2.0)/ph);
                if(!std::isfinite(center.x())||!std::isfinite(center.y()))
                    return fail(QStringLiteral("Vị trí ảnh gốc không hợp lệ."));
            }else{
                w=widthPercent==0?double(matrix.a):pw*widthPercent/100;
                h=w*matrix.d/matrix.a;
            }
        }else{
            w=pw*widthPercent/100;h=w*image.height()/image.width();
        }
        const double scale=qMin(1.0,qMin(pw/w,ph/h));w*=scale;h*=scale;
        if(!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0)return fail(QStringLiteral("Kích thước ảnh không hợp lệ."));
        x=qBound(0.0,center.x()*pw-w/2,qMax(0.0,pw-w));
        y=qBound(0.0,(1-center.y())*ph-h/2,qMax(0.0,ph-h));
        const auto oldObject=object;
        if(!moving||replacing){
            object=FPDFPageObj_NewImageObj(doc.p);
            if(!object)return fail(QStringLiteral("Không tạo được đối tượng ảnh."));
            // PDFium copies the supplied BGRA bitmap into the image object.
            auto bitmap=FPDFBitmap_CreateEx(image.width(),image.height(),FPDFBitmap_BGRA,
                const_cast<uchar*>(image.constBits()),image.bytesPerLine());
            if(!bitmap){FPDFPageObj_Destroy(object);return fail(QStringLiteral("Không tạo được bitmap."));}
            const bool set=FPDFImageObj_SetBitmap(nullptr,0,object,bitmap);
            FPDFBitmap_Destroy(bitmap);
            if(!set){FPDFPageObj_Destroy(object);return fail(QStringLiteral("Không đọc được nội dung ảnh."));}
        }
        if(!FPDFImageObj_SetMatrix(object,w,0,0,h,x,y)){
            if(!moving||replacing)FPDFPageObj_Destroy(object);
            return fail(QStringLiteral("Không đặt được ảnh."));
        }
        if(replacing){
            if(!FPDFPage_RemoveObject(pg.p,oldObject)){
                FPDFPageObj_Destroy(object);
                return fail(QStringLiteral("Không tách được ảnh cũ."));
            }
            FPDFPageObj_Destroy(oldObject);
            // PDFium 8179 API transfers ownership even if insertion fails.
            if(!FPDFPage_InsertObjectAtIndex(pg.p,object,size_t(selectedIndex)))
                return fail(QStringLiteral("Không chèn được ảnh tại lớp cũ."));
        }else if(!moving)FPDFPage_InsertObject(pg.p,object);
        if(!FPDFPage_GenerateContent(pg.p))return fail(QStringLiteral("Không cập nhật được nội dung trang."));
    }
    QTemporaryDir temp(QDir(target.absolutePath()).filePath(QStringLiteral("~dung-image-XXXXXX")));
    if(!temp.isValid())return fail(QStringLiteral("Không tạo được thư mục tạm."));
    const auto path=temp.filePath(QStringLiteral("image.pdf"));QFile out(path);
    if(!out.open(QIODevice::WriteOnly))return fail(QStringLiteral("Không ghi được PDF."));
    Writer writer(&out);const bool saved=FPDF_SaveAsCopy(doc.p,&writer,0)&&out.flush();out.close();
    if(!saved||out.error()!=QFileDevice::NoError)return fail(QStringLiteral("Không lưu được PDF."));
    QFile check(path);
    if(!check.open(QIODevice::ReadOnly)||check.size()>256LL*1024*1024)return fail(QStringLiteral("Không đọc lại được PDF."));
    const auto data=check.readAll();check.close();
    {
        Doc verify{FPDF_LoadMemDocument64(data.constData(),size_t(data.size()),nullptr)};
        if(!verify.p||FPDF_GetPageCount(verify.p)!=pages)return fail(QStringLiteral("Số trang sau khi lưu không khớp."));
        Page pg{FPDF_LoadPage(verify.p,page-1)};
        if(!pg.p||FPDFPage_CountObjects(pg.p)!=originalCount+(moving?0:1))return fail(QStringLiteral("Không xác minh được số đối tượng."));
        Text text{FPDFText_LoadPage(pg.p)};
        if(!text.p||texts(pg.p,text.p)!=originalText)return fail(QStringLiteral("Lớp chữ sau khi lưu không khớp."));
        int found=0,images=0;
        for(int i=0;i<FPDFPage_CountObjects(pg.p);++i){
            auto obj=FPDFPage_GetObject(pg.p,i);
            if(!obj||FPDFPageObj_GetType(obj)!=FPDF_PAGEOBJ_IMAGE)continue;
            ++images;
            float l=0,b=0,r=0,t=0;
            if(FPDFPageObj_GetBounds(obj,&l,&b,&r,&t)&&std::abs(l-x)<.1&&std::abs(b-y)<.1&&
                std::abs(r-x-w)<.1&&std::abs(t-y-h)<.1){
                if(replacing){
                    unsigned int iw=0,ih=0;
                    if(!FPDFImageObj_GetImagePixelSize(obj,&iw,&ih)||iw!=unsigned(image.width())||
                        ih!=unsigned(image.height())||i!=selectedIndex)continue;
                }
                ++found;
            }
        }
        if(found!=1||images!=originalImages+(moving?0:1))return fail(QStringLiteral("Không xác minh được vị trí ảnh sau khi lưu."));
    }
    const auto inf=dungpdf::info(path);
    if(!inf.ok||inf.pages!=pages||dungpdf::fingerprint(source)!=expectedSource||
       QFileInfo::exists(dest)||QFileInfo(dest).isSymLink()||!QFile::rename(path,dest))
        return fail(QStringLiteral("Không xác minh hoặc công bố được PDF mới."));
    dungpdf::Result r;r.ok=true;r.files<<target.absoluteFilePath();
    r.warnings<<QStringLiteral("Ảnh nằm trên nội dung hiện có; hãy kiểm tra bố cục. Đây không phải xóa thông tin mật.");
    return r;
#endif
}
dungpdf::Result insertImage(const QString& source,int page,const QString& imagePath,QPointF center,
                           double widthPercent,const QByteArray& expectedSource,const QString& dest){
    return editImage(source,page,imagePath,std::nullopt,center,widthPercent,expectedSource,dest);
}
dungpdf::Result moveImage(const QString& source,int page,QPointF selected,QPointF center,
                          double widthPercent,const QByteArray& expectedSource,const QString& dest){
    return editImage(source,page,QString(),selected,center,widthPercent,expectedSource,dest);
}
dungpdf::Result replaceImage(const QString& source,int page,QPointF selected,const QString& imagePath,
                             const QByteArray& expectedSource,const QString& dest){
    if(imagePath.isEmpty())return fail(QStringLiteral("Hãy chọn ảnh thay thế."));
    return editImage(source,page,imagePath,selected,selected,0,expectedSource,dest);
}
dungpdf::Result replaceObject(const QString& source,int page,const QString& oldText,const QString& replacement,const QString& dest,const QByteArray& expectedSource,std::optional<QPointF> selected){
#ifndef DUNG_HAVE_PDFIUM_EDIT
    return fail(QStringLiteral("Bản cài chưa có PDFium Editing SDK."));
#else
    const auto newText=replacement.normalized(QString::NormalizationForm_C);
    if(selected&&(!std::isfinite(selected->x())||!std::isfinite(selected->y())||selected->x()<0||selected->x()>1||selected->y()<0||selected->y()>1))
        return fail(QStringLiteral("Vị trí chọn chữ không hợp lệ."));
    if(oldText==newText||!editableText(oldText)||!editableText(newText))
        return fail(QStringLiteral("Chọn một đoạn chữ in được trên một dòng (tối đa 500 ký tự)."));
    const QFileInfo target(dest);
    if(target.exists()||target.isSymLink()||!QDir(target.absolutePath()).exists())return fail(QStringLiteral("Chọn tên tệp mới."));
    QFile input(source);
    if(!input.open(QIODevice::ReadOnly)||input.size()<=0||input.size()>128LL*1024*1024)return fail(QStringLiteral("Nguồn không hợp lệ hoặc vượt 128 MiB."));
    const QByteArray bytes=input.readAll();
    if(input.error()!=QFileDevice::NoError||bytes.size()!=input.size())return fail(QStringLiteral("Không đọc đủ PDF."));
    input.close();
    if(!expectedSource.isEmpty()&&(expectedSource.size()!=32||
       QCryptographicHash::hash(bytes,QCryptographicHash::Sha256)!=expectedSource))
        return fail(QStringLiteral("PDF nguồn đã thay đổi. Hãy chọn lại chữ."));
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
        // Only use normalized viewer geometry for ordinary unrotated pages.
        const double pw=FPDF_GetPageWidthF(pg.p),ph=FPDF_GetPageHeightF(pg.p);
        FS_RECTF box{};
        const bool locate=selected&&FPDFPage_GetRotation(pg.p)==0&&FPDF_GetPageBoundingBox(pg.p,&box)&&
            std::isfinite(pw)&&std::isfinite(ph)&&pw>0&&ph>0&&std::abs(box.left)<.01&&std::abs(box.bottom)<.01&&
            std::abs(box.right-pw)<.01&&std::abs(box.top-ph)<.01;
        {
            Text tp{FPDFText_LoadPage(pg.p)};
            if(!tp.p)return fail(QStringLiteral("Không đọc được chữ."));
            int count=FPDFPage_CountObjects(pg.p),idx=-1;
            if(count<1||count>50000)return fail(QStringLiteral("Trang quá phức tạp."));
            for(int i=0;i<count;++i){
                auto o=FPDFPage_GetObject(pg.p,i);
                if(!o||FPDFPageObj_GetType(o)!=FPDF_PAGEOBJ_TEXT)continue;
                const auto s=readText(o,tp.p);
                bool hit=s==oldText;
                if(hit&&locate){
                    float l=0,b=0,r=0,t=0;
                    const double x=selected->x()*pw,y=(1-selected->y())*ph;
                    hit=FPDFPageObj_GetBounds(o,&l,&b,&r,&t)&&x>=l&&x<=r&&y>=b&&y<=t;
                }
                if(hit){
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
    if(!info.ok||info.pages!=pages||(!expectedSource.isEmpty()&&dungpdf::fingerprint(source)!=expectedSource)||
       QFileInfo::exists(dest)||QFileInfo(dest).isSymLink()||!QFile::rename(path,dest))
        return fail(QStringLiteral("Không xác minh hoặc công bố được PDF mới."));
    dungpdf::Result r;r.ok=true;r.files<<target.absoluteFilePath();r.warnings<<QStringLiteral("Kiểm tra bố cục khi độ dài chữ thay đổi.");return r;
#endif
}
}
