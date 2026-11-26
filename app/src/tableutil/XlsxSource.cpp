// SPDX-License-Identifier: MPL-2.0
#include "XlsxSource.h"
#include "../tools/filetools/ZipLite.h"
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QMap>
#include <QObject>
#include <QFileInfo>
namespace tbl {
namespace {
const auto ns=QStringLiteral("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
const auto relNs=QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");
const auto packageNs=QStringLiteral("http://schemas.openxmlformats.org/package/2006/relationships");
bool fail(QString& error,const QString& why){error=why;return false;}
bool safeXml(const QByteArray& bytes){
    return !bytes.isEmpty() && bytes.size()<=32*1024*1024 && !bytes.contains("<!DOCTYPE") && !bytes.contains("<!ENTITY");
}
bool readPart(const ft::ZipReader& zip,const QString& path,QByteArray& bytes,QString& error,bool optional=false){
    const ft::ZipEntryInfo* found=nullptr;
    for(const auto& entry:zip.entries())if(entry.name==path){
        if(found)return fail(error,QObject::tr("Gói XLSX có phần trùng tên."));
        found=&entry;
    }
    if(!found){
        if(optional){bytes.clear();return true;}
        return fail(error,QObject::tr("Thiếu thành phần XLSX: %1").arg(path));
    }
    return zip.read(*found,&bytes,&error,32LL*1024*1024);
}
bool openZip(const QString& path,ft::ZipReader& zip,QString& error){
    const QFileInfo f(path);
    if(!f.isFile() || f.size()>100LL*1024*1024 || !path.endsWith(QLatin1String(".xlsx"),Qt::CaseInsensitive))
        return fail(error,QObject::tr("Chọn XLSX thường, tối đa 100 MB."));
    return zip.open(path,&error);
}
bool catalog(const ft::ZipReader& zip,QList<XlsxSheet>& sheets,QString& error){
    QByteArray book,rels;
    return readPart(zip,QStringLiteral("xl/workbook.xml"),book,error)
        && readPart(zip,QStringLiteral("xl/_rels/workbook.xml.rels"),rels,error)
        && parseXlsxSheets(book,rels,sheets,error);
}
// Accept only A1 references within the supported rectangular import window.
bool address(const QString& ref,int& row,int& col){
    static const QRegularExpression rx(QStringLiteral(R"(\A([A-Z]{1,3})([1-9][0-9]{0,6})\z)"));
    const auto m=rx.match(ref);if(!m.hasMatch())return false;
    row=m.captured(2).toInt();col=0;
    for(auto ch:m.captured(1))col=col*26+ch.unicode()-'A'+1;
    return row>=1 && row<=50001 && col>=1 && col<=256;
}
// OOXML escaped UTF-16 code units, including escaped literal escape sequences.
QString unescape(QString text){
    static const QRegularExpression rx(QStringLiteral(R"(_x([0-9a-fA-F]{4})_)"));
    QString out;qsizetype pos=0;
    auto it=rx.globalMatch(text);
    while(it.hasNext()){
        const auto m=it.next();
        out+=text.mid(pos,m.capturedStart()-pos);
        out+=QChar(ushort(m.captured(1).toUInt(nullptr,16)));
        pos=m.capturedEnd();
    }
    return out+text.mid(pos);
}
bool richText(QXmlStreamReader& x,QString& value){
    // At <si> or <is>. Only direct t and r/t; phonetic annotations ignored.
    while(x.readNextStartElement()){
        if(x.namespaceUri()!=ns){x.skipCurrentElement();continue;}
        if(x.name()==QLatin1String("t"))value+=x.readElementText();
        else if(x.name()==QLatin1String("r")){
            while(x.readNextStartElement()){
                if(x.namespaceUri()==ns && x.name()==QLatin1String("t"))value+=x.readElementText();
                else x.skipCurrentElement();
            }
        }else x.skipCurrentElement();
        if(value.size()>1048576)return false;
    }
    value=unescape(value);
    return !x.hasError();
}
bool sharedStrings(const QByteArray& xml,QStringList& strings,QString& error){
    if(xml.isEmpty())return true;
    if(!safeXml(xml))return fail(error,QObject::tr("Bảng chuỗi XLSX không hợp lệ."));
    QXmlStreamReader x(xml);
    if(!x.readNextStartElement() || x.namespaceUri()!=ns || x.name()!=QLatin1String("sst"))
        return fail(error,QObject::tr("Thiếu bảng chuỗi XLSX hợp lệ."));
    while(x.readNextStartElement()){
        if(x.namespaceUri()==ns && x.name()==QLatin1String("si")){
            QString value;if(strings.size()>=1000000 || !richText(x,value))return fail(error,QObject::tr("Bảng chuỗi quá lớn hoặc lỗi."));
            strings<<value;
        }else x.skipCurrentElement();
    }
    while(!x.atEnd())x.readNext();
    return !x.hasError() || fail(error,x.errorString());
}
}
bool parseXlsxSheets(const QByteArray& workbook,const QByteArray& relations,QList<XlsxSheet>& sheets,QString& error){
    error.clear();
    if(!safeXml(workbook)||!safeXml(relations))return fail(error,QObject::tr("XML workbook không hợp lệ."));
    QHash<QString,QString> targets;QSet<QString> ids;
    QXmlStreamReader r(relations);
    if(!r.readNextStartElement()||r.namespaceUri()!=packageNs||r.name()!=QLatin1String("Relationships"))return fail(error,QObject::tr("Thiếu workbook relationships."));
    static const QRegularExpression partRx(QStringLiteral(R"(\Axl/worksheets/[A-Za-z0-9_.-]+\.xml\z)"));
    while(r.readNextStartElement()){
        if(r.namespaceUri()==packageNs&&r.name()==QLatin1String("Relationship")){
            const auto a=r.attributes();const auto id=a.value("Id").toString();
            if(id.isEmpty()||ids.contains(id))return fail(error,QObject::tr("Relationship ID không hợp lệ."));
            ids.insert(id);
            if(a.value("Type")==relNs+QStringLiteral("/worksheet") && a.value("TargetMode")!=QLatin1String("External")){
                auto target=a.value("Target").toString();
                target=target.startsWith(QLatin1Char('/'))?target.mid(1):QStringLiteral("xl/")+target;
                if(!partRx.match(target).hasMatch())return fail(error,QObject::tr("Đường dẫn worksheet không được hỗ trợ."));
                targets.insert(id,target);
            }
        }
        r.skipCurrentElement();
    }
    while(!r.atEnd())r.readNext();
    if(r.hasError())return fail(error,r.errorString());
    QList<XlsxSheet> out;QSet<QString> seen;
    QXmlStreamReader x(workbook);
    if(!x.readNextStartElement()||x.namespaceUri()!=ns||x.name()!=QLatin1String("workbook"))return fail(error,QObject::tr("Workbook không được hỗ trợ."));
    while(x.readNextStartElement()){
        if(x.namespaceUri()!=ns||x.name()!=QLatin1String("sheets")){x.skipCurrentElement();continue;}
        while(x.readNextStartElement()){
            if(x.namespaceUri()==ns&&x.name()==QLatin1String("sheet")){
                auto id=x.attributes().value(relNs,QStringLiteral("id")).toString();
                auto name=x.attributes().value("name").toString();
                if(targets.contains(id)){
                    if(name.isEmpty()||seen.contains(id)||out.size()>=1024)return fail(error,QObject::tr("Danh sách sheet không hợp lệ."));
                    seen.insert(id);out.append(XlsxSheet{name,targets.value(id)});
                }
            }
            x.skipCurrentElement();
        }
    }
    while(!x.atEnd())x.readNext();
    if(x.hasError())return fail(error,x.errorString());
    if(out.isEmpty())return fail(error,QObject::tr("Không có worksheet được hỗ trợ."));
    sheets=out;return true;
}
bool xlsxSheets(const QString& path,QList<XlsxSheet>& sheets,QString& error){
    error.clear();ft::ZipReader zip;
    return openZip(path,zip,error)&&catalog(zip,sheets,error);
}
bool parseXlsxTable(const QByteArray& sheet,const QByteArray& shared,Table& table,QString& error){
    error.clear();
    if(!safeXml(sheet))return fail(error,QObject::tr("XML sheet không hợp lệ."));
    QStringList strings;if(!sharedStrings(shared,strings,error))return false;
    QMap<int,QMap<int,QString>> rows;int maxRow=0,maxCol=0;
    QXmlStreamReader x(sheet);
    if(!x.readNextStartElement()||x.namespaceUri()!=ns||x.name()!=QLatin1String("worksheet"))return fail(error,QObject::tr("Worksheet không được hỗ trợ."));
    bool hasData=false;
    while(x.readNextStartElement()){
        if(x.namespaceUri()!=ns||x.name()!=QLatin1String("sheetData")){x.skipCurrentElement();continue;}
        if(hasData)return fail(error,QObject::tr("Sheet có dữ liệu trùng phần."));
        hasData=true;
        int prevRow=0;
        while(x.readNextStartElement()){
            if(x.namespaceUri()!=ns||x.name()!=QLatin1String("row")){x.skipCurrentElement();continue;}
            bool ok=false;int row=x.attributes().value("r").toInt(&ok);
            if(!ok||row<=prevRow||row>50001)return fail(error,QObject::tr("Số dòng sheet không hợp lệ hoặc vượt 50.000 dòng dữ liệu."));
            prevRow=row;int prevCol=0;
            while(x.readNextStartElement()){
                if(x.namespaceUri()!=ns||x.name()!=QLatin1String("c")){x.skipCurrentElement();continue;}
                int rr=0,col=0;
                if(!address(x.attributes().value("r").toString(),rr,col)||rr!=row||col<=prevCol)
                    return fail(error,QObject::tr("Địa chỉ ô không hợp lệ hoặc vượt 256 cột."));
                prevCol=col;
                const auto type=x.attributes().value("t").toString();
                if(!type.isEmpty()&&type!=QLatin1String("n")&&type!=QLatin1String("s")&&type!=QLatin1String("inlineStr")&&type!=QLatin1String("b")&&type!=QLatin1String("d")&&type!=QLatin1String("str"))
                    return fail(error,QObject::tr("Ô %1 có kiểu lỗi hoặc chưa được hỗ trợ.").arg(col));
                QString value;bool hasValue=false;
                while(x.readNextStartElement()){
                    if(x.namespaceUri()!=ns){x.skipCurrentElement();continue;}
                    if(x.name()==QLatin1String("f"))return fail(error,QObject::tr("Sheet có công thức; hãy tạo bản sao chỉ chứa giá trị trước khi nhập."));
                    if(x.name()==QLatin1String("v")){
                        if(hasValue)return fail(error,QObject::tr("Ô có nhiều giá trị."));
                        hasValue=true;value=x.readElementText();
                    }else if(x.name()==QLatin1String("is")){
                        if(hasValue||type!=QLatin1String("inlineStr")||!richText(x,value))return fail(error,QObject::tr("Chuỗi nội tuyến không hợp lệ."));
                        hasValue=true;
                    }else x.skipCurrentElement();
                }
                if(x.hasError())return fail(error,x.errorString());
                if(!hasValue)continue; // style-only cells are not table data
                if(type==QLatin1String("s")){
                    bool valid=false;const auto index=value.toInt(&valid);
                    if(!valid||index<0||index>=strings.size())return fail(error,QObject::tr("Chỉ số chuỗi dùng chung sai."));
                    value=strings[index];
                }else if(type==QLatin1String("b")){
                    if(value!=QLatin1String("0")&&value!=QLatin1String("1"))return fail(error,QObject::tr("Giá trị boolean sai."));
                    value=value==QLatin1String("1")?QStringLiteral("TRUE"):QStringLiteral("FALSE");
                }else if(type==QLatin1String("str"))value=unescape(value);
                if(value.size()>1048576)return fail(error,QObject::tr("Ô quá dài."));
                maxRow=qMax(maxRow,row);maxCol=qMax(maxCol,col);
                if(qint64(maxRow)*maxCol>5000000)return fail(error,QObject::tr("Vùng dữ liệu vượt 5 triệu ô."));
                rows[row].insert(col,value);
            }
        }
    }
    while(!x.atEnd())x.readNext();
    if(x.hasError())return fail(error,x.errorString());
    if(!hasData||maxCol==0||!rows.contains(1))return fail(error,QObject::tr("Hàng 1 phải chứa tiêu đề."));
    Table out;QSet<QString> names;
    for(int col=1;col<=maxCol;++col){
        const auto name=rows.value(1).value(col);
        if(name.trimmed().isEmpty()||names.contains(name.trimmed().toCaseFolded()))
            return fail(error,QObject::tr("Tiêu đề hàng 1 thiếu hoặc trùng tên cột."));
        names.insert(name.trimmed().toCaseFolded());out.header<<name;
    }
    for(int row=2;row<=maxRow;++row){
        QStringList line;const auto cells=rows.value(row);
        for(int col=1;col<=maxCol;++col)line<<cells.value(col);
        out.rows.append(line);
    }
    table=out;return true;
}
bool readXlsxTable(const QString& path,const XlsxSheet& sheet,Table& table,QString& error){
    error.clear();ft::ZipReader zip;QList<XlsxSheet> sheets;
    if(!openZip(path,zip,error)||!catalog(zip,sheets,error))return false;
    bool found=false;
    for(const auto& s:sheets)if(s.name==sheet.name&&s.part==sheet.part){found=true;break;}
    if(!found)return fail(error,QObject::tr("Sheet đã thay đổi; chọn lại tệp."));
    QByteArray xml,shared;
    return readPart(zip,sheet.part,xml,error)&&readPart(zip,QStringLiteral("xl/sharedStrings.xml"),shared,error,true)
        &&parseXlsxTable(xml,shared,table,error);
}
}
