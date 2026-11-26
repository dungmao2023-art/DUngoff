// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "TableOps.h"
namespace tbl {
struct XlsxSheet { QString name, part; };
// Only ordinary internal worksheet parts. No external relationship is followed.
bool xlsxSheets(const QString& path,QList<XlsxSheet>& sheets,QString& error);
bool parseXlsxSheets(const QByteArray& workbook,const QByteArray& relations,QList<XlsxSheet>& sheets,QString& error);
bool parseXlsxTable(const QByteArray& sheet,const QByteArray& shared,Table& table,QString& error);
// Raw stored values, first physical row is header. Formula/error cells are refused.
bool readXlsxTable(const QString& path,const XlsxSheet& sheet,Table& table,QString& error);
}
