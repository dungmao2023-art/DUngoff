// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "../tools/filetools/ZipLite.h"
#include "../word/PageMargins.h"
#include <QDir>
#include <QHash>
#include <QSet>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QStringList>

namespace excelrules {
inline const QString ns=QStringLiteral("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
inline const QString rel=QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");
inline bool fail(QString* err,const QString& s){if(err)*err=s;return false;}
inline bool read(ft::ZipReader& z,const QString& name,QByteArray* out,QString* err){
    const auto* item=z.find(name);
    return item?z.read(*item,out,err,32LL*1024*1024):fail(err,QStringLiteral("Thiếu phần XLSX: ")+name);
}
inline bool patch(const QByteArray& source,const QString& range,const QStringList& values,bool protect,QByteArray* output,QString* err){
    if(!output)return false;
    if(!protect){
        static const QRegularExpression address(QStringLiteral("^[A-Z]{1,3}[1-9][0-9]{0,6}(:[A-Z]{1,3}[1-9][0-9]{0,6})?$"));
        if(!address.match(range).hasMatch()||values.size()<2||values.size()>100)
            return fail(err,QStringLiteral("Cần vùng hợp lệ và 2–100 lựa chọn."));
        QSet<QString> unique;
        for(const auto& v:values){
            if(v.trimmed().isEmpty()||v.contains(QRegularExpression(QStringLiteral("[,;\"\\r\\n\\x00-\\x1f]")))||unique.contains(v))
                return fail(err,QStringLiteral("Mỗi lựa chọn phải khác nhau, không chứa dấu phẩy, chấm phẩy, nháy kép hoặc ký tự điều khiển."));
            unique.insert(v);
        }
        if(values.join(QLatin1Char(',')).size()>253)
            return fail(err,QStringLiteral("Danh sách trực tiếp tối đa 253 ký tự."));
    }
    const QString text=QString::fromUtf8(source);
    if(text.toUtf8()!=source)return fail(err,QStringLiteral("XML không phải UTF-8."));
    QXmlStreamReader x(text);int depth=0;qsizetype insertion=-1;bool root=false,seenData=false;
    const QSet<QString> afterProtection={QStringLiteral("protectedRanges"),QStringLiteral("scenarios"),QStringLiteral("autoFilter"),QStringLiteral("sortState"),QStringLiteral("dataConsolidate"),QStringLiteral("customSheetViews"),QStringLiteral("mergeCells"),QStringLiteral("phoneticPr"),QStringLiteral("conditionalFormatting"),QStringLiteral("dataValidations")};
    const QSet<QString> afterValidation={QStringLiteral("hyperlinks"),QStringLiteral("printOptions"),QStringLiteral("pageMargins"),QStringLiteral("pageSetup"),QStringLiteral("headerFooter"),QStringLiteral("rowBreaks"),QStringLiteral("colBreaks"),QStringLiteral("customProperties"),QStringLiteral("cellWatches"),QStringLiteral("ignoredErrors"),QStringLiteral("smartTags"),QStringLiteral("drawing"),QStringLiteral("legacyDrawing"),QStringLiteral("legacyDrawingHF"),QStringLiteral("picture"),QStringLiteral("oleObjects"),QStringLiteral("controls"),QStringLiteral("webPublishItems"),QStringLiteral("tableParts"),QStringLiteral("extLst")};
    auto start=[&]()->qsizetype{const auto off=x.characterOffset();return off>0&&off<=text.size()?text.lastIndexOf(QLatin1Char('<'),off-1):-1;};
    while(!x.atEnd()){
        x.readNext();if(x.isDTD())return fail(err,QStringLiteral("Không hỗ trợ DTD."));
        if(x.isStartElement()){
            ++depth;
            if(depth==1){root=x.namespaceUri()==ns&&x.name()==QLatin1String("worksheet");if(!root)return fail(err,QStringLiteral("Không phải worksheet chuẩn."));}
            if(depth==2){
                if(x.namespaceUri()!=ns)return fail(err,QStringLiteral("Worksheet mở rộng chưa hỗ trợ."));
                const QString name=x.name().toString();
                if(name==QLatin1String("sheetData"))seenData=true;
                if(name==QLatin1String("sheetProtection"))return fail(err,QStringLiteral("Sheet đã được bảo vệ; chưa sửa quy tắc."));
                if(!protect&&name==QLatin1String("dataValidations"))return fail(err,QStringLiteral("Sheet đã có quy tắc xác thực; chưa hỗ trợ sửa hoặc ghép."));
                if(seenData&&insertion<0&&(afterValidation.contains(name)||(protect&&afterProtection.contains(name))))insertion=start();
                if(!protect&&name==QLatin1String("extLst"))return fail(err,QStringLiteral("Sheet có quy tắc mở rộng; chưa hỗ trợ."));
            }
        }else if(x.isEndElement()){
            if(depth==1&&insertion<0)insertion=start();
            --depth;
        }
    }
    if(x.hasError()||!root||!seenData||insertion<0||insertion>text.size())return fail(err,QStringLiteral("Không xác minh được vị trí quy tắc."));
    QString fragment;
    if(protect)fragment=QStringLiteral("<d:sheetProtection xmlns:d=\"%1\" sheet=\"1\" objects=\"1\" scenarios=\"1\"/>").arg(ns);
    else fragment=QStringLiteral("<d:dataValidations xmlns:d=\"%1\" count=\"1\"><d:dataValidation type=\"list\" allowBlank=\"1\" showErrorMessage=\"1\" errorStyle=\"stop\" sqref=\"%2\"><d:formula1>\"%3\"</d:formula1></d:dataValidation></d:dataValidations>").arg(ns,range,values.join(QLatin1Char(',')).toHtmlEscaped());
    QString result=text;result.insert(insertion,fragment);
    QXmlStreamReader verify(result);while(!verify.atEnd())verify.readNext();
    if(verify.hasError())return fail(err,verify.errorString());
    *output=result.toUtf8();return true;
}
inline bool createCopy(const QString& src,const QString& dst,int index,const QString& name,const QString& range,const QStringList& values,bool protect,QString* err){
    ft::ZipReader z;if(!z.open(src,err))return false;
    QSet<QString> names;
    for(const auto& e:z.entries()){
        if(names.contains(e.name)||e.name.startsWith(QLatin1String("_xmlsignatures/"),Qt::CaseInsensitive))
            return fail(err,QStringLiteral("Gói có mục trùng hoặc chữ ký số."));
        names.insert(e.name);
    }
    QByteArray book,rels;
    if(!read(z,QStringLiteral("xl/workbook.xml"),&book,err)||!read(z,QStringLiteral("xl/_rels/workbook.xml.rels"),&rels,err))return false;
    QXmlStreamReader w(book);int i=0;QString id;
    while(!w.atEnd()){
        w.readNext();if(w.isDTD())return false;
        if(w.isStartElement()&&w.namespaceUri()==ns&&w.name()==QLatin1String("sheet")){
            if(i++==index&&w.attributes().value(QStringLiteral("name"))==name)id=w.attributes().value(rel,QStringLiteral("id")).toString();
        }
    }
    if(w.hasError()||id.isEmpty())return fail(err,QStringLiteral("Không xác minh được sheet nguồn."));
    QXmlStreamReader rr(rels);QString entry;
    while(!rr.atEnd()){
        rr.readNext();if(rr.isDTD())return false;
        if(!rr.isStartElement()||rr.namespaceUri()!=QLatin1String("http://schemas.openxmlformats.org/package/2006/relationships")||rr.name()!=QLatin1String("Relationship"))continue;
        const auto a=rr.attributes();if(a.value(QStringLiteral("Id"))!=id)continue;
        if(!entry.isEmpty()||a.value(QStringLiteral("TargetMode"))==QLatin1String("External")||a.value(QStringLiteral("Type"))!=rel+QStringLiteral("/worksheet"))return fail(err,QStringLiteral("Quan hệ sheet không hợp lệ."));
        const QString raw=a.value(QStringLiteral("Target")).toString();
        entry=QDir::cleanPath(raw.startsWith(QLatin1Char('/'))?raw.mid(1):QStringLiteral("xl/")+raw);
    }
    if(rr.hasError()||!entry.startsWith(QLatin1String("xl/worksheets/"))||!entry.endsWith(QLatin1String(".xml"))||entry.contains(QLatin1Char('\\')))
        return fail(err,QStringLiteral("Không xác minh được XML sheet."));
    QByteArray xml,out;
    return read(z,entry,&xml,err)&&patch(xml,range,values,protect,&out,err)&&dung::word::replaceZipEntry(src,dst,entry,out,err);
}
}
