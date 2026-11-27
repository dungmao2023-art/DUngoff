// SPDX-License-Identifier: MPL-2.0
// Kiểm thử tiện ích bảng tính: dòng trùng + hoàn tác, so sánh hai bảng, làm sạch ngày/tiền.
#include "TableOps.h"
#include "XlsxSource.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

using namespace tbl;
static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) { ++g_pass; std::cout << "[ĐẠT]  " << QString(msg).toStdString() << "\n"; } \
    else { ++g_fail; std::cout << "[LỖI]  " << QString(msg).toStdString() << "  (" #cond ")\n"; } } while (0)

static Table T(const QStringList &h, const QVector<QStringList> &r) { Table t; t.header = h; t.rows = r; return t; }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // v5: join first, then group by a right-hand column; exact schema required.
    {
        const Table left=T({"ID","Amount"},{{"001","0.1"},{"001","0.2"},{"1","3"},{"002","4"}});
        const Table right=T({"ID","Region"},{{"001","A"},{"1","B"}});
        QueryJoinSpec join;join.enabled=true;join.leftKey=0;join.rightKey=0;join.rightHeader=right.header;
        const auto input=queryJoinInput(left,right,join);
        CHECK(input.ok&&input.matched==3&&input.unmatched==1&&input.table.header.value(2)=="B.Region","v5 left join preserves exact codes");
        FilterSortSpec steps;steps.groupKey=2;steps.groupValue=1;
        const auto result=filterSort(input.table,steps);
        CHECK(result.ok&&result.table.rows.size()==3&&result.table.rows.value(0).value(3)=="0.3","group by joined region");
        const auto json=encodeQueryRecipe(left.header,join,steps);
        QueryJoinSpec loaded;FilterSortSpec decoded;QString error;
        CHECK(decodeQueryRecipe(json,left.header,right.header,loaded,decoded,error)&&loaded.enabled&&decoded.groupKey==2&&filterSort(queryJoinInput(left,right,loaded).table,decoded).table==result.table,"v5 recipe replay");
        CHECK(!decodeQueryRecipe(json,left.header,{"ID","Other"},loaded,decoded,error)&&loaded.enabled&&decoded.groupKey==2,"schema mismatch preserves current options");
        steps.groupExtrema=true;
        const auto extended=filterSort(input.table,steps);
        CHECK(extended.ok&&extended.table.rows.value(0).value(4)=="0.1"&&extended.table.rows.value(0).value(5)=="0.2","joined query extrema");
        const auto newJson=encodeQueryRecipe(left.header,join,steps);
        CHECK(decodeQueryRecipe(newJson,left.header,right.header,loaded,decoded,error)&&decoded.groupExtrema&&
              filterSort(queryJoinInput(left,right,loaded).table,decoded).table==extended.table,"v5 join wrapper with v6 extrema replays");
        CHECK(decodeQueryRecipe(json,left.header,right.header,loaded,decoded,error)&&!decoded.groupExtrema&&
              filterSort(input.table,decoded).table==result.table,"old join recipe retains old schema");
        FilterSortSpec only;only.groupKey=0;only.groupValue=1;only.groupExtrema=true;
        const auto standalone=encodeFilterSortRecipe(left.header,only);
        CHECK(decodeQueryRecipe(standalone,left.header,{},loaded,decoded,error)&&!loaded.enabled&&decoded.groupExtrema,
              "v6 without join loads");
        auto invalid=standalone;invalid.replace("\"groupValue\": 1","\"groupValue\": -1");
        CHECK(invalid!=standalone&&!decodeQueryRecipe(invalid,left.header,{},loaded,decoded,error)&&decoded.groupExtrema,
              "reject extrema without numeric column, preserve prior config");
        invalid=standalone;invalid.replace("\"version\": 6","\"version\": 4");
        CHECK(invalid!=standalone&&!decodeQueryRecipe(invalid,left.header,{},loaded,decoded,error),
              "older recipe version cannot silently ignore extrema");
        CHECK(!queryJoinInput(left,T({"ID","Other"},{{"001","A"}}),join).ok,"reject changed lookup schema");
        CHECK(!queryJoinInput(left,T(right.header,{{"001","A"},{"001","B"}}),join).ok,"reject duplicate right keys");
        steps.groupAverage=true;
        const auto avgJson=encodeQueryRecipe(left.header,join,steps);
        CHECK(decodeQueryRecipe(avgJson,left.header,right.header,loaded,decoded,error)&&decoded.groupAverage&&decoded.groupExtrema&&
            filterSort(input.table,decoded).table.rows.value(0).value(6)=="0.15","v7 average and extrema replay in joined query");
        CHECK(decodeQueryRecipe(newJson,left.header,right.header,loaded,decoded,error)&&!decoded.groupAverage&&decoded.groupExtrema,"v6 recipe does not add average");
        steps.groupValue=-1;
        CHECK(!filterSort(input.table,steps).ok,"average needs a numeric column");
        join.kind=JoinKind::Inner;
        CHECK(queryJoinInput(left,right,join).table.rows.size()==3,"inner join excludes missing keys");
        CHECK(decodeQueryRecipe(encodeFilterSortRecipe(left.header,FilterSortSpec{}),left.header,{},loaded,decoded,error)&&!loaded.enabled,"legacy recipe remains compatible");
    }

    // Composite keys must be injective even with literal separators in input.
    {
        const QString sep(QChar(0x1F));
        const QString nul(QChar(0));
        const Table tricky=T({"A","B"},{{"a"+sep+"b","c"},{"a","b"+sep+"c"},
            {"a"+sep+"b","c"},{"",""},{"",""},{nul,"1:2"},{"",nul+"1:2"}});
        const Table before=tricky;
        const auto result=removeDuplicates(tricky,{0,1},false);
        CHECK(result.ok && result.cleaned.rows.size()==6 && result.removed.size()==1 &&
            result.removed.first().originalIndex==2 && result.removed.first().keptIndex==0,
            "composite key collision does not remove distinct rows");
        CHECK(result.cleaned.rows.value(1)==tricky.rows.value(1) &&
            result.cleaned.rows.value(4)==tricky.rows.value(5) && result.cleaned.rows.value(5)==tricky.rows.value(6),
            "embedded separators and NUL remain distinct");
        CHECK(tricky==before,"source unchanged after dedup");
        const auto repeated=removeDuplicates(tricky,{0,0},false);
        CHECK(!repeated.ok && repeated.cleaned==tricky && repeated.removed.isEmpty(),"reject repeated key column without data loss");
        const Table ragged=T({"A","B"},{{"x"},{"x",""}});
        const auto malformed=removeDuplicates(ragged,{0,1},false);
        CHECK(!malformed.ok && malformed.cleaned==ragged && malformed.removed.isEmpty(),"reject malformed row instead of treating missing cell as blank");
        const auto folded=removeDuplicates(T({"A","B"},{{" ID "," Tên "},{"id","tên"}}),{0,1},true);
        CHECK(folded.ok && folded.removed.size()==1,"trim and case-fold still work");
    }

    // Import resource bounds must refuse without returning a partial table.
    {
        const Table before=T({"ID"},{{"keep"}});
        Table result=before;QString error;
        QByteArray longCell("ID\n");longCell+=QByteArray(kMaxCsvCellChars+1,'x');
        CHECK(!readCsvBytes(longCell,Encoding::Utf8,result,error)&&result==before&&!error.isEmpty(),"reject oversized CSV cell without changing result");
        const QByteArray quoted="ID\n\""+QByteArray(kMaxCsvCellChars+1,'x')+"\"";
        CHECK(!readCsvBytes(quoted,Encoding::Utf8,result,error)&&result==before,"quoted field obeys same limit");
        QByteArray columns(kMaxCsvColumns,',');columns+="x\n";
        CHECK(!readCsvBytes(columns,Encoding::Utf8,result,error)&&result==before,"reject excessive CSV columns");
        const QByteArray rows=QByteArray("x\n").repeated(kMaxCsvRecords+1);
        CHECK(!readCsvBytes(rows,Encoding::Utf8,result,error)&&result==before,"reject excessive records without truncation");
        QTemporaryDir dir;QFile huge(dir.filePath("large.csv"));
        const bool created=huge.open(QIODevice::WriteOnly)&&huge.resize(kMaxCsvBytes+1);
        huge.close();
        QByteArray raw("previous");
        CHECK(created&&!readCsvFileBytes(huge.fileName(),raw,error)&&raw=="previous","reject oversized file before reading, preserve prior bytes");
        CHECK(!readCsvFileBytes(dir.path(),raw,error)&&raw=="previous","reject directory as CSV source");
        CHECK(readCsvBytes(QByteArray("ID\n")+QByteArray(kMaxCsvCellChars,'x'),Encoding::Utf8,result,error)&&
            result.rows.size()==1&&result.rows.first().first().size()==kMaxCsvCellChars,"exact field boundary accepted");
    }

    // ---- Dòng trùng ----
    Table t = T({"Ma", "Ten", "SDT"}, {
        {"A01", "Nguyễn Văn An", "0912345678"},
        {"a01 ", "Nguyễn Văn An (trùng hoa/thường)", "0900000000"},
        {"B02", "Trần Thị Bình", "0987654321"},
        {"A01", "Nguyễn Văn An", "0912345678"},
        {"", "Dòng không mã 1", ""},
        {"", "Dòng không mã 2", ""},
    });
    const Table orig = t;
    auto d1 = removeDuplicates(t, {0}, false);
    CHECK(d1.ok && d1.cleaned.rows.size() == 5 && d1.removed.size() == 1 && d1.removed[0].originalIndex == 3, "trùng đúng khoá, phân biệt hoa/thường khi không chuẩn hoá");
    CHECK(d1.removed[0].keptIndex == 0, "diff ghi dòng được giữ");
    auto d2 = removeDuplicates(t, {0}, true);
    CHECK(d2.removed.size() == 2 && d2.cleaned.rows.size() == 4, "chuẩn hoá hoa/thường/khoảng trắng bắt thêm 'a01 '");
    CHECK(d2.cleaned.rows[3][1] == "Dòng không mã 1" || d2.cleaned.rows.contains(QStringList{"", "Dòng không mã 2", ""}), "dòng khoá rỗng không bị coi là trùng");
    CHECK(!d2.warnings.isEmpty(), "có cảnh báo về khoá rỗng");
    CHECK(undoDedup(d2) == orig, "hoàn tác dòng trùng == bảng gốc, từng ô");
    CHECK(undoDedup(d1) == orig, "hoàn tác (không chuẩn hoá) == bảng gốc");
    CHECK(t == orig, "bảng gốc không bị sửa");
    auto d3 = removeDuplicates(t, {0, 2}, false);
    CHECK(d3.removed.size() == 1, "khoá nhiều cột");
    CHECK(!removeDuplicates(t, {}, false).ok && !removeDuplicates(t, {9}, false).ok, "khoá sai bị từ chối");
    CHECK(t.rows[0][2] == "0912345678" && d1.cleaned.rows[0][2] == "0912345678", "số điện thoại giữ số 0 đầu");

    // ---- Lọc và sắp xếp dạng chữ, không sửa nguồn ----
    {
        const Table src = T({"ID", "City", "Phone"}, {
            {"010", "An Khê", "0912345678"}, {"002", "an khê", "0123"},
            {"002", "An Khê", "0456"}, {"001", "Huế", "9999999999999999"},
            {"003", "", "0000"}});
        FilterSortSpec spec; spec.filterCol = 1; spec.value = QStringLiteral("an khê");
        spec.ignoreCase = true; spec.sortCol = 0;
        auto r = filterSort(src, spec);
        CHECK(r.ok && r.table.rows.size() == 3, "lọc Unicode không phân biệt hoa thường");
        CHECK(r.table.rows.value(0).value(2) == "0123" && r.table.rows.value(1).value(2) == "0456", "sort ổn định, giữ số 0 đầu");
        CHECK(r.table.rows.value(2).value(0) == "010" && src.rows.value(0).value(0) == "010", "không sửa nguồn");
        spec.descending = true; r = filterSort(src, spec);
        CHECK(r.table.rows.value(0).value(0) == "010", "sort giảm dần");
        spec.mode = FilterMode::Blank; r = filterSort(src, spec);
        CHECK(r.ok && r.table.rows.size() == 1 && r.table.rows.value(0).value(2) == "0000", "lọc ô trống giữ mã");
        spec.mode = FilterMode::Contains; spec.value.clear();
        CHECK(!filterSort(src, spec).ok, "không chấp nhận contains rỗng");
        spec.filterCol = 99;
        CHECK(!filterSort(src, spec).ok, "từ chối chỉ số cột lỗi");
        spec.filterCol = -1; spec.sortCol = -1;
        CHECK(filterSort(src, spec).table == src, "không chọn bước thì giữ nguyên bảng");
        spec.filterCol = 1; spec.sortCol = 0; spec.mode = FilterMode::Equal;
        spec.value = QStringLiteral("An Khê"); spec.descending = true;
        const auto json = encodeFilterSortRecipe(src.header, spec);
        FilterSortSpec loaded; QString error;
        CHECK(decodeFilterSortRecipe(json, src.header, loaded, error), "nạp lại cấu hình JSON");
        CHECK(filterSort(src, loaded).table == filterSort(src, spec).table, "chạy lại cho kết quả giống hệt");
        CHECK(!json.contains("0912345678"), "cấu hình không chứa dòng dữ liệu");
        const QStringList reordered{"City", "ID", "Phone"};
        CHECK(!decodeFilterSortRecipe(json, reordered, loaded, error), "từ chối cột đổi thứ tự");
        CHECK(!decodeFilterSortRecipe(QByteArray("{bad"), src.header, loaded, error), "từ chối JSON hỏng");
        CHECK(loaded.filterCol == spec.filterCol && loaded.value == spec.value, "cấu hình lỗi không ghi đè lựa chọn hiện tại");
        auto invalid = json; invalid.replace("\"version\": 1", "\"version\": 2");
        CHECK(!decodeFilterSortRecipe(invalid, src.header, loaded, error), "từ chối cấu hình v2 thiếu cột");
        spec.outputColumns={2,0}; spec.outputNames={"Điện thoại","Mã"};
        const auto projected=filterSort(src,spec);
        CHECK(projected.ok && projected.table.header==spec.outputNames && projected.table.rows.size()==3,"project selected columns after filter");
        CHECK(projected.table.rows.value(0).value(0)=="0912345678" && projected.table.rows.value(0).value(1)=="010","projection follows sort and preserves text");
        const auto v2=encodeFilterSortRecipe(src.header,spec);
        CHECK(decodeFilterSortRecipe(v2,src.header,loaded,error) && filterSort(src,loaded).table==projected.table,"v2 recipe roundtrip");
        CHECK(decodeFilterSortRecipe(json,src.header,loaded,error) && loaded.outputColumns.isEmpty(),"v1 recipe remains compatible");
        auto bad=spec; bad.outputNames={"Mã","mã"};
        CHECK(!filterSort(src,bad).ok,"reject duplicate output names");
        bad=spec; bad.outputColumns={2,2};
        CHECK(!filterSort(src,bad).ok,"reject duplicate source columns");
        bad=spec; bad.outputColumns={2,99};
        CHECK(!filterSort(src,bad).ok,"reject invalid source column");
        bad=spec; bad.outputNames={"","Mã"};
        CHECK(!filterSort(src,bad).ok,"reject empty output name");
        auto ragged=src; ragged.rows.append(QStringList{"001"});
        CHECK(!filterSort(ragged,spec).ok,"reject projection of ragged rows");
        FilterSortSpec onlyFilter;
        onlyFilter.filterCol=1; onlyFilter.mode=FilterMode::Blank;
        const auto badFilter=filterSort(ragged,onlyFilter);
        CHECK(!badFilter.ok&&badFilter.table.rows.isEmpty()&&!badFilter.error.isEmpty(),
            "blank filter rejects missing field rather than treating it as blank");
        FilterSortSpec onlySort; onlySort.sortCol=1;
        CHECK(!filterSort(ragged,onlySort).ok,"sort rejects short row");
        auto oversized=src; auto extra=src.rows.first(); extra.append("unexpected");
        oversized.rows.append(extra);
        CHECK(!filterSort(oversized,onlySort).ok,"sort rejects extra field");
        CHECK(!filterSort(ragged,FilterSortSpec{}).ok,"no-op recipe rejects malformed input");
        CHECK(filterSort(src,FilterSortSpec{}).table==src,"valid no-op still preserves data");
    }

    // ---- Fixed-order query pipeline, with v3 recipes ----
    {
        const Table src=T({"ID","Amount","City"},{{"001","1.234,50","X"},{"001","99","X"},{"002","1.234,50","Y"},{"","1.234,50","X"},{"","1.234,50","X"}});
        const Table before=src;
        FilterSortSpec p;p.cleanCol=1;p.cleanKind=ColumnKind::Money;p.filterCol=1;p.value="1234.5";
        p.dedupCol=0;p.sortCol=0;p.outputColumns={1,0};p.outputNames={"Tien","Ma"};
        auto r=filterSort(src,p);
        CHECK(r.ok && r.table.rows.size()==4 && r.filteredRows==1 && r.removedDuplicates==0,"clean before filter, retain blank keys");
        CHECK(r.table.rows.value(2).value(1)=="001" && r.table.rows.value(2).value(0)=="1234.5" && src==before,"sort/project preserves IDs and source");
        p.filterCol=-1;r=filterSort(src,p);
        CHECK(r.ok && r.removedDuplicates==1 && r.table.rows.size()==4 && r.table.rows.value(2).value(0)=="1234.5","dedup keeps first source record");
        const auto json=encodeFilterSortRecipe(src.header,p);FilterSortSpec decoded;QString error;
        CHECK(decodeFilterSortRecipe(json,src.header,decoded,error) && filterSort(src,decoded).table==r.table,"v3 pipeline recipe roundtrip");
        CHECK(decoded.cleanCol==1 && decoded.dedupCol==0 && decoded.cleanKind==ColumnKind::Money,"v3 settings restored");
        p.outputColumns.clear();p.outputNames.clear();
        CHECK(decodeFilterSortRecipe(encodeFilterSortRecipe(src.header,p),src.header,decoded,error)&&decoded.outputColumns.isEmpty(),"v3 without projection");
        p.cleanCol=-1;CHECK(!filterSort(src,p).ok,"reject invalid cleaning spec");
        p.cleanCol=99;CHECK(!filterSort(src,p).ok,"reject invalid clean column");
        p.cleanCol=1;p.dedupCol=99;CHECK(!filterSort(src,p).ok,"reject invalid dedup column");
    }

    // Boolean normalization is opt-in, text-only, and precedes filters.
    {
        const Table src=T({"ID","Flag"},{{"001"," Có "},{"002","FALSE"},{"003","YES"},{"004","không"},{"005","001"},{"006","1"},{"007",""},{"008","maybe"},{"009","ĐÚNG"},{"010","Sai"}});
        const Table before=src;
        FilterSortSpec p;p.cleanCol=1;p.cleanKind=ColumnKind::Boolean;
        const auto r=filterSort(src,p);
        CHECK(r.ok&&r.cleanedCells==5&&r.cleanWarnings==3,"boolean normalization counts only actual changes and unknowns");
        CHECK(r.table.rows.value(0).value(1)=="TRUE"&&r.table.rows.value(1).value(1)=="FALSE"&&r.table.rows.value(3).value(1)=="FALSE","Vietnamese and English boolean values");
        CHECK(r.table.rows.value(4).value(1)=="001"&&r.table.rows.value(5).value(1)=="1"&&r.table.rows.value(6).value(1).isEmpty()&&r.table.rows.value(7).value(1)=="maybe"&&src==before,"unknowns codes blanks and original source preserved");
        const auto standalone=cleanTable(src,{{"Flag",ColumnKind::Boolean}});
        CHECK(standalone.cleaned==r.table&&standalone.changes.size()==5&&standalone.warnings.size()==3,"same explicit semantics in clean-table UI");
        p.filterCol=1;p.value="TRUE";
        const auto filtered=filterSort(src,p);
        CHECK(filtered.ok&&filtered.table.rows.size()==3&&filtered.table.rows.value(2).value(0)=="009","boolean cleaning runs before filtering");
        FilterSortSpec decoded;QString error;
        CHECK(decodeFilterSortRecipe(encodeFilterSortRecipe(src.header,p),src.header,decoded,error)&&decoded.cleanKind==ColumnKind::Boolean&&filterSort(src,decoded).table==filtered.table,"recipe boolean roundtrip");
        CHECK(!cleanBoolean("0").converted&&!cleanBoolean("0").warning.isEmpty(),"numeric zero not inferred");
        CHECK(!cleanBoolean("TRUE").converted&&cleanBoolean("TRUE").warning.isEmpty(),"canonical boolean unchanged");
        p.cleanKind=static_cast<ColumnKind>(99);
        CHECK(!filterSort(src,p).ok,"invalid kind rejected");
        CHECK(!decodeFilterSortRecipe(encodeFilterSortRecipe(src.header,p),src.header,decoded,error)&&decoded.cleanKind==ColumnKind::Boolean,"invalid recipe leaves prior configuration intact");
    }

    // ---- Ghép bảng ----
    {
        const auto left=T({"ID","B.Name"},{{"001","x"},{"1","y"},{"001","z"},{"","empty"}});
        const auto right=T({"Code","Name","Phone"},{{"001","An","0912345678"},{"","blank","999"}});
        const auto leftCopy=left,rightCopy=right;
        auto r=joinLookup(left,right,0,0,JoinKind::Left);
        CHECK(r.ok && r.table.rows.size()==4 && r.matched==2 && r.unmatched==2,"left join preserves row count");
        CHECK(r.table.rows.value(0).value(3)=="0912345678" && r.table.rows.value(1).value(3).isEmpty(),"exact keys preserve leading zeros");
        CHECK(r.table.header.value(2)=="B.Name_2","unique column name");
        CHECK(r.blankLeft==1 && r.blankRight==1 && r.table.rows.value(3).value(2).isEmpty(),"blank keys never match");
        CHECK(r.table.rows.value(2).value(1)=="z" && left==leftCopy && right==rightCopy,"left order and sources preserved");
        auto inner=joinLookup(left,right,0,0,JoinKind::Inner);
        CHECK(inner.ok && inner.table.rows.size()==2 && inner.table.rows.value(1).value(1)=="z","inner join keeps matched rows only");
        auto duplicate=right; duplicate.rows.append(QStringList{"001","Other","222"});
        CHECK(!joinLookup(left,duplicate,0,0,JoinKind::Left).ok,"reject duplicate lookup key");
        auto malformed=right; malformed.rows.append(QStringList{"002"});
        CHECK(!joinLookup(left,malformed,0,0,JoinKind::Left).ok,"reject ragged right row");
        CHECK(!joinLookup(left,right,99,0,JoinKind::Left).ok,"reject invalid key column");
        auto noMatch=joinLookup(T({"ID"},{{"404"}}),right,0,0,JoinKind::Inner);
        CHECK(noMatch.ok && noMatch.table.rows.isEmpty() && noMatch.table.header.size()==3,"empty result retains schema");
    }

    // ---- So sánh ----
    Table a = T({"Ma", "Ten", "Tien"}, {{"1", "An", "1000"}, {"2", "Bình", "2000"}, {"3", "Chi", "3000"}, {"3", "Chi trùng", "3"}});
    Table b = T({"Ma", "Ten", "Tien", "GhiChu"}, {{"1", "An", "1000", ""}, {"2", "Bình", "2500", ""}, {"4", "Dũng", "4000", ""}});
    auto c = compareTables(a, b, "Ma");
    CHECK(c.ok && c.same == 1, "một dòng giống hệt");
    CHECK(c.changed.size() == 1 && c.changed[0].key == "2" && c.changed[0].column == "Tien" && c.changed[0].valueA == "2000" && c.changed[0].valueB == "2500", "phát hiện ô khác giá trị");
    CHECK(c.onlyInA.size() == 1 && c.onlyInA[0][0] == "3" && c.onlyInB.size() == 1 && c.onlyInB[0][0] == "4", "dòng chỉ có ở A và ở B");
    QString allw = c.warnings.join("\n");
    CHECK(allw.contains("Mã trùng trong bảng A") && allw.contains("GhiChu"), "cảnh báo mã trùng và cột chỉ có ở một bên");
    Table dt = diffAsTable(c);
    CHECK(dt.rows.size() == 3 && dt.header.size() == 5, "bảng khác biệt xuất ra đủ 3 dòng");
    CHECK(!compareTables(a, b, "KhongCo").ok, "cột mã không tồn tại bị từ chối");
    Table z1 = T({"Ma", "V"}, {{"1", "007"}}), z2 = T({"Ma", "V"}, {{"1", "7"}});
    CHECK(compareTables(z1, z2, "Ma").changed.size() == 1, "'007' khác '7' (không ép kiểu số)");
    Table e1 = T({"Ma", "V"}, {{"1", "12345678901234567890"}}), e2 = T({"Ma", "V"}, {{"1", "12345678901234567891"}});
    CHECK(compareTables(e1, e2, "Ma").changed.size() == 1, "số dài 20 chữ số so chính xác, không mất độ chính xác");

    // ---- Ngày ----
    CHECK(cleanDate("5/3/2026").value == "05/03/2026" && cleanDate("5/3/2026").converted, "ngày d/m/yyyy -> dd/mm/yyyy");
    CHECK(cleanDate("2026-03-05").value == "05/03/2026", "ngày ISO -> dd/mm/yyyy");
    CHECK(cleanDate("05.03.2026").value == "05/03/2026" && cleanDate("05-03-2026").value == "05/03/2026", "ngày dấu chấm/gạch");
    CHECK(!cleanDate("31/02/2026").converted && !cleanDate("31/02/2026").warning.isEmpty(), "ngày không tồn tại: giữ nguyên + cảnh báo");
    CHECK(!cleanDate("5/3/26").converted && !cleanDate("5/3/26").warning.isEmpty(), "năm 2 chữ số: không đoán");
    CHECK(!cleanDate("45000").converted && !cleanDate("0912345678").converted, "số seri/số điện thoại không bị đổi thành ngày");
    CHECK(cleanDate("05/03/2026").converted == false && cleanDate("05/03/2026").value == "05/03/2026", "đã chuẩn thì không đổi");
    CHECK(cleanDate("").value.isEmpty() && cleanDate("xin chào").value == "xin chào", "ô rỗng/chữ giữ nguyên");

    // ---- Tiền ----
    CHECK(cleanMoney("1.500.000").value == "1500000", "VN: 1.500.000 -> 1500000");
    CHECK(cleanMoney("1.500.000đ").value == "1500000" && cleanMoney("1.500.000 VNĐ").value == "1500000" && cleanMoney("1.500.000 ₫").value == "1500000", "bỏ ký hiệu tiền ở cuối");
    CHECK(cleanMoney("1.500.000,50").value == "1500000.5", "VN: dấu phẩy thập phân, bỏ 0 thừa, không làm tròn");
    CHECK(cleanMoney("1,500,000.25", MoneyLocale::EN).value == "1500000.25", "EN: 1,500,000.25");
    CHECK(cleanMoney("-2.000").value == "-2000" && cleanMoney("(2.000)").value == "-2000", "số âm");
    CHECK(!cleanMoney("1.50.000").converted && !cleanMoney("1.50.000").warning.isEmpty(), "dấu nghìn sai vị trí: giữ nguyên + cảnh báo");
    CHECK(!cleanMoney("1,5,0").converted, "phân cách lạ không đoán");
    CHECK(!cleanMoney("0912345678").converted && !cleanMoney("0912345678").warning.isEmpty(), "số điện thoại không bị coi là tiền");
    CHECK(!cleanMoney("1234567890123456789").converted, "số >15 chữ số giữ nguyên (không qua double)");
    CHECK(cleanMoney("12abc").value == "12abc" && !cleanMoney("12abc").warning.isEmpty(), "chữ lẫn số giữ nguyên");
    CHECK(cleanMoney("0,5").value == "0.5", "0,5 hợp lệ");
    CHECK(isProtectedText("007") && isProtectedText("12345678901234567") && !isProtectedText("0") && !isProtectedText("1500"), "nhận diện chuỗi cần bảo vệ");

    // ---- Làm sạch cả bảng + CSV ----
    Table raw = T({"Ma", "Ngay", "Tien"}, {
        {"001", "5/3/2026", "1.500.000đ"}, {"002", "31/02/2026", "abc"}, {"0912345678", "2026-12-01", "2.000"}});
    const Table rawCopy = raw;
    auto cr = cleanTable(raw, {{"Ngay", ColumnKind::Date}, {"Tien", ColumnKind::Money}, {"Ma", ColumnKind::Keep}, {"Khong", ColumnKind::Date}});
    CHECK(raw == rawCopy, "bảng gốc không bị sửa khi làm sạch");
    CHECK(cr.changes.size() == 4, "đúng số ô được đổi");
    CHECK(cr.cleaned.rows[0][0] == "001" && cr.cleaned.rows[2][0] == "0912345678", "cột mã giữ nguyên số 0 đầu");
    CHECK(cr.cleaned.rows[1][1] == "31/02/2026" && cr.cleaned.rows[1][2] == "abc", "ô lỗi giữ nguyên");
    CHECK(cr.warnings.size() == 3, "mỗi ô không chuyển được và cột thiếu đều có cảnh báo");

    QTemporaryDir dir;
    const QString p1 = dir.filePath("vào.csv"), p2 = dir.filePath("ra.csv");
    {
        QFile f(p1); f.open(QIODevice::WriteOnly);
        f.write("\xEF\xBB\xBFMa,Ten,Ghi chu\n001,\"Nguyễn, Văn \"\"An\"\"\",\"dòng1\ndòng2\"\n0912345678,Trần,\n");
    }
    Table rd; QString err;
    CHECK(readCsv(p1, rd, err) && rd.rows.size() == 2 && rd.rows[0][0] == "001" && rd.rows[0][1] == "Nguyễn, Văn \"An\"" && rd.rows[0][2] == "dòng1\ndòng2", "đọc CSV: BOM, dấu phẩy/nháy/xuống dòng trong ô, số 0 đầu");
    CHECK(writeCsvCopy(p2, rd, err), "ghi bản sao CSV");
    Table rd2; CHECK(readCsv(p2, rd2, err) && rd2 == rd, "ghi rồi đọc lại == nguyên vẹn");
    CHECK(!writeCsvCopy(p2, rd, err) && !err.isEmpty(), "không ghi đè tệp đã có");
    Table existing; CHECK(readCsv(p2, existing, err) && existing == rd, "tệp có sẵn không đổi sau từ chối ghi");
    {
        Table parsed;
        CHECK(readCsvBytes("ID,Name\r001,An\r002,Binh\r", Encoding::Utf8, parsed, err)
            && parsed.rows.size() == 2 && parsed.rows.value(1).value(0) == "002", "CSV xuống dòng CR đơn");
        const Table before = parsed;
        CHECK(!readCsvBytes("ID,Name\n001,a\"b\"\n", Encoding::Utf8, parsed, err) && parsed == before,
            "từ chối dấu nháy giữa ô, giữ kết quả cũ");
        CHECK(!readCsvBytes("ID,Name\n001,\"An\"x\n", Encoding::Utf8, parsed, err) && parsed == before,
            "từ chối chữ sau dấu nháy đóng");
        CHECK(!readCsvBytes("ID,Name\n001,\"An\n", Encoding::Utf8, parsed, err) && parsed == before,
            "từ chối dấu nháy chưa đóng");
        CHECK(readCsvBytes("ID,Name\r\n001,\"a\r\nb\"\r\n", Encoding::Utf8, parsed, err)
            && parsed.rows.value(0).value(1) == "a\r\nb", "giữ nguyên CRLF bên trong ô");
        const auto p3 = dir.filePath("crlf.csv");
        CHECK(writeCsvCopy(p3, parsed, err) && err.isEmpty(), "ghi bản sao xóa lỗi cũ");
        Table round; CHECK(readCsv(p3, round, err) && round == parsed, "roundtrip CRLF và số 0 đầu");
        CHECK(!writeCsvCopy(dir.filePath("empty.csv"), Table{}, err), "không xuất bảng thiếu tiêu đề");
        CHECK(!readCsvBytes("ID|Name", Encoding::Utf8, parsed, err, QLatin1Char('|')), "từ chối phân cách không hỗ trợ");
    }
    QFile bad(dir.filePath("hong.csv")); bad.open(QIODevice::WriteOnly); bad.write("Ma\n\xFF\xFE\xFD\n"); bad.close();
    Table rb; CHECK(!readCsv(dir.filePath("hong.csv"), rb, err), "CSV không phải UTF-8: từ chối, không đoán bảng mã");
    CHECK(!readCsv(dir.filePath("khong-co.csv"), rb, err), "tệp không tồn tại báo lỗi");


    // ---- Nhận diện bảng mã CSV ----
    {
        const QString vi = QStringLiteral("Họ tên,Địa chỉ\n001,Nguyễn Văn An\n");
        const QByteArray u8 = vi.toUtf8();
        const QByteArray cp = QByteArray::fromHex("486ff22074ea6e2cd069f26120636869d20a3030312c4e677579eade6e2056e36e20416e0a");
        QString txt, e2;
        auto g = detectCsvEncoding(QByteArray("\xEF\xBB\xBF") + u8);
        CHECK(g.enc == Encoding::Utf8Bom && g.confidence == 1.0 && !g.needsUserChoice, "BOM UTF-8: chắc chắn, không cần hỏi");
        g = detectCsvEncoding(u8);
        CHECK(g.enc == Encoding::Utf8 && g.confidence >= 0.95 && !g.needsUserChoice, "UTF-8 không BOM có tiếng Việt: nhận ra");
        g = detectCsvEncoding("Ma,Ten\n1,An\n");
        CHECK(g.enc == Encoding::Utf8 && g.confidence == 1.0, "ASCII thuần: UTF-8");
        QByteArray le("\xFF\xFE"), be("\xFE\xFF");
        for (QChar c : vi) { le.append(char(c.unicode() & 0xFF)); le.append(char(c.unicode() >> 8)); be.append(char(c.unicode() >> 8)); be.append(char(c.unicode() & 0xFF)); }
        g = detectCsvEncoding(le); CHECK(g.enc == Encoding::Utf16LE && !g.needsUserChoice, "BOM UTF-16 LE");
        g = detectCsvEncoding(be); CHECK(g.enc == Encoding::Utf16BE && !g.needsUserChoice, "BOM UTF-16 BE");
        CHECK(decodeText(le, Encoding::Utf16LE, txt, e2) && txt == vi, "giải mã UTF-16 LE đúng nguyên văn");
        CHECK(decodeText(be, Encoding::Utf16BE, txt, e2) && txt == vi, "giải mã UTF-16 BE đúng nguyên văn");
        g = detectCsvEncoding(cp);
        CHECK(g.enc == Encoding::Windows1258 && g.needsUserChoice && g.confidence < 0.95 && !g.reason.isEmpty(), "Windows-1258: chỉ gợi ý, BẮT người dùng xác nhận");
        CHECK(decodeText(cp, Encoding::Windows1258, txt, e2) && txt == vi, "giải mã Windows-1258 ra đúng chữ Việt (NFC)");
        g = detectCsvEncoding(QByteArray("\xA9\xB5\xAE\xB1 abc\n"));
        CHECK(g.needsUserChoice && g.confidence < 0.5, "byte không giống tiếng Việt: độ tin cậy thấp, cần người dùng chọn");
        g = detectCsvEncoding(QByteArray("Ma\n\x81\x8A\n"));
        CHECK(g.enc == Encoding::Unknown && g.needsUserChoice, "byte không định nghĩa: không đoán");
        CHECK(!decodeText(QByteArray("Ma\n\x81\n"), Encoding::Windows1258, txt, e2) && !e2.isEmpty(), "Windows-1258 gặp byte lạ: từ chối, không thay ký tự");
        CHECK(!decodeText(cp, Encoding::Utf8, txt, e2), "chọn nhầm UTF-8 cho tệp Windows-1258: từ chối");
        CHECK(!decodeText(QByteArray("abc"), Encoding::Utf16LE, txt, e2), "UTF-16 độ dài lẻ: từ chối");
        QByteArray nobom; for (QChar c : QStringLiteral("Ma,Ten\n001,Nam\n002,Hoa\n")) { nobom.append(char(c.unicode())); nobom.append('\0'); }
        g = detectCsvEncoding(nobom);
        CHECK(g.enc == Encoding::Utf16LE && g.needsUserChoice && g.confidence < 0.95, "UTF-16 không BOM: gợi ý và bắt xác nhận");

        QString big; for (int i = 0; i < 25; ++i) big += QStringLiteral("d%1;x\r\n").arg(i);
        const QStringList pv = previewLines(big, 20);
        CHECK(pv.size() == 20 && pv[0] == "d0;x" && !pv[0].contains(QLatin1Char('\r')), "xem trước đúng 20 dòng, bỏ \\r");
        CHECK(guessSeparator(big) == QLatin1Char(';') && guessSeparator("a,b\n") == QLatin1Char(',') && guessSeparator("a\tb\n") == QLatin1Char('\t'), "đoán dấu phân cách");

        QTemporaryDir d2;
        const QString pc = d2.filePath("cu.csv");
        { QFile f(pc); f.open(QIODevice::WriteOnly); f.write(cp); }
        Table t1; QString er;
        CHECK(!readCsv(pc, t1, er), "readCsv mặc định (UTF-8) vẫn từ chối tệp Windows-1258");
        CHECK(readCsvWith(pc, Encoding::Windows1258, t1, er) && t1.header.value(1) == QStringLiteral("Địa chỉ") && t1.rows.value(0).value(1) == QStringLiteral("Nguyễn Văn An") && t1.rows[0][0] == "001", "đọc CSV Windows-1258 sau khi chọn bảng mã");
        QFile chk(pc); chk.open(QIODevice::ReadOnly);
        CHECK(chk.readAll() == cp, "tệp gốc không bị đổi byte nào");
        const QString outp = d2.filePath("moi.csv");
        CHECK(writeCsvCopy(outp, t1, er), "ghi bản sao UTF-8 từ tệp Windows-1258");
        Table t2; CHECK(readCsv(outp, t2, er) && t2 == t1, "bản sao đọc lại nguyên vẹn");
    }

    // ---- Exact decimal group aggregation ----
    {
        const Table src=T({"ID","Amount"},{{"001","0.1"},{"001","0.2"},{"1","-0.0001"},{"",""},{"A","1.2345"},{"A","-1.2345"}});
        const Table before=src;
        auto r=groupCsv(src,0,1);
        CHECK(r.ok && r.table.rows.size()==4 && r.table.rows.value(0).value(3)=="0.3","exact decimal sum");
        CHECK(r.table.rows.value(1).value(0)=="1" && r.table.rows.value(1).value(3)=="-0.0001","preserve key and negative fraction");
        CHECK(r.table.rows.value(2).value(0).isEmpty() && r.table.rows.value(2).value(2)=="0" && r.table.rows.value(2).value(3).isEmpty(),"blank key grouped, blank value excluded");
        CHECK(r.table.rows.value(3).value(3)=="0" && src==before,"cancellation exact, source unchanged");
        const auto extrema=groupCsv(src,0,1,true);
        CHECK(extrema.ok&&extrema.table.header.size()==6&&extrema.table.rows.value(0).value(4)=="0.1"&&extrema.table.rows.value(0).value(5)=="0.2","exact min max positive decimals");
        CHECK(extrema.table.rows.value(1).value(4)=="-0.0001"&&extrema.table.rows.value(1).value(5)=="-0.0001","single negative fraction");
        CHECK(extrema.table.rows.value(2).value(4).isEmpty()&&extrema.table.rows.value(2).value(5).isEmpty(),"empty numeric group has no extrema");
        CHECK(extrema.table.rows.value(3).value(4)=="-1.2345"&&extrema.table.rows.value(3).value(5)=="1.2345","mixed signs compare numerically");
        CHECK(groupCsv(src,0,1).table==r.table&&groupCsv(src,0,-1,true).table.header.size()==2,"legacy schema and count-only unchanged");
        const auto avg=groupCsv(T({"ID","Amount"},{{"P","0.0001"},{"P","0"},{"P",""},{"N","-0.0001"},{"N","0"},{"Z",""},{"R","1"},{"R","2"},{"R","2"}}),0,1,false,true);
        CHECK(avg.ok&&avg.table.header.size()==5&&avg.table.rows.value(0).value(4)=="0.0001","positive tie rounds away from zero, excludes blank");
        CHECK(avg.table.rows.value(1).value(4)=="-0.0001","negative tie rounds away from zero");
        CHECK(avg.table.rows.value(2).value(4).isEmpty(),"no numeric values means blank average");
        CHECK(avg.table.rows.value(3).value(4)=="1.6667","recurring average rounds to four places");
        const auto both=groupCsv(src,0,1,true,true);
        CHECK(both.ok&&both.table.header.size()==7&&both.table.rows.value(0).value(6)=="0.15","min max and average coexist");
        auto count=groupCsv(src,0);
        CHECK(count.ok && count.table.rows.value(0).value(1)=="2" && count.table.header.size()==2,"count only");
        for(const QString& bad:QStringList{"001","1,2","1.00001","1e2"," 1","999999999999999","=1+2","1\n"}) {
            CHECK(!groupCsv(T({"ID","Amount"},{{"A",bad}}),0,1).ok,"reject noncanonical or out of range number");
        }
        CHECK(!groupCsv(T({"ID","Amount"},{{"A"}}),0,1).ok,"reject ragged input");
        CHECK(!groupCsv(src,0,0).ok && !groupCsv(src,99,1).ok,"reject invalid columns");
        Table huge=T({"ID","Amount"},{});
        for(int i=0;i<10;++i)huge.rows.append(QStringList{"A","99999999999999.9999"});
        CHECK(!groupCsv(huge,0,1).ok,"reject sum overflow");
    }

    // v4 groups the projected result while preserving source-index identity.
    {
        const Table src=T({"ID","Money","City"},{{"001","0,10","X"},{"001","0,20","X"},{"1","12,3456","Y"},{"002","0","X"}});
        FilterSortSpec p;p.cleanCol=1;p.cleanKind=ColumnKind::Money;p.filterCol=2;p.value="X";
        p.outputColumns={1,0};p.outputNames={"Tien","Ma"};p.groupKey=0;p.groupValue=1;
        const auto result=filterSort(src,p);
        CHECK(result.ok&&result.grouped&&result.groupedInputRows==3&&result.table.rows.size()==2&&
            result.table.rows.value(0).value(0)=="001"&&result.table.rows.value(0).value(3)=="0.3","group after cleaning filtering and reordered projection");
        CHECK(result.table.rows.value(1).value(3)=="0"&&src.rows.value(0).value(1)=="0,10","group preserves zero and source");
        FilterSortSpec loaded;QString error;
        CHECK(decodeFilterSortRecipe(encodeFilterSortRecipe(src.header,p),src.header,loaded,error)&&
            loaded.groupKey==0&&loaded.groupValue==1&&filterSort(src,loaded).table==result.table,"v4 replay exact output");
        p.outputColumns={0};p.outputNames={"Ma"};
        CHECK(!filterSort(src,p).ok,"group rejects dropped numeric column");
        p.groupValue=-1;
        CHECK(filterSort(src,p).ok&&filterSort(src,p).table.header.size()==2,"group count only after projection");
        p.groupKey=-1;p.groupValue=1;
        CHECK(!filterSort(src,p).ok,"reject sum without grouping key");
        p.groupKey=0;p.groupValue=0;
        CHECK(!filterSort(src,p).ok,"reject same group and value column");
        p.groupValue=-1;p.filterCol=2;p.value="absent";
        CHECK(filterSort(src,p).ok&&filterSort(src,p).table.rows.isEmpty(),"empty group output retains schema");
    }

    // XLSX worksheet XML: read-only raw-value subset, no implicit evaluation.
    {
        const QByteArray book=R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Second" r:id="r2"/><sheet name="First" r:id="r1"/></sheets></workbook>)";
        const QByteArray rels=R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="r1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="r2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="/xl/worksheets/sheet2.xml"/></Relationships>)";
        QList<XlsxSheet> sheets;QString error;
        CHECK(parseXlsxSheets(book,rels,sheets,error) && sheets.size()==2 && sheets.value(0).name=="Second" && sheets.value(1).part=="xl/worksheets/sheet1.xml","XLSX workbook order and relationship target");
        auto badRels=rels;badRels.replace("worksheets/sheet1.xml","../evil.xml");
        CHECK(!parseXlsxSheets(book,badRels,sheets,error),"reject unsafe worksheet target");
        const QByteArray shared=R"(<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><si><t>ID</t></si><si><r><t>Điện</t></r><r><t xml:space="preserve"> thoại</t></r><rPh><t>ignored</t></rPh></si><si><t>001</t></si></sst>)";
        const QByteArray sheet=R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData><row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1" t="s"><v>1</v></c></row><row r="2"><c r="A2" t="s"><v>2</v></c><c r="B2" t="inlineStr"><is><t>0912345678</t></is></c></row><row r="4"><c r="A4"><v>123.45</v></c><c r="B4" t="b"><v>1</v></c></row></sheetData></worksheet>)";
        Table table;
        CHECK(parseXlsxTable(sheet,shared,table,error) && table.header.value(1)=="Điện thoại","rich shared string without phonetic annotation");
        CHECK(table.rows.size()==3 && table.rows.value(0).value(0)=="001" && table.rows.value(0).value(1)=="0912345678","preserve stored text and leading zeros");
        CHECK(table.rows.value(1).value(0).isEmpty() && table.rows.value(2).value(0)=="123.45" && table.rows.value(2).value(1)=="TRUE","sparse rows and raw values");
        const auto before=table;
        auto formula=sheet;formula.replace("<v>123.45</v>","<f>1+2</f><v>3</v>");
        CHECK(!parseXlsxTable(formula,shared,table,error) && table==before,"refuse formulas and preserve prior result");
        auto badIndex=sheet;badIndex.replace("<v>2</v>","<v>99</v>");
        CHECK(!parseXlsxTable(badIndex,shared,table,error),"reject missing shared string");
        auto far=sheet;far.replace("B4","XFD4");
        CHECK(!parseXlsxTable(far,shared,table,error),"reject oversized column");
        CHECK(!parseXlsxTable(sheet.left(50),shared,table,error),"reject malformed XML");
    }

    // Full ZIP/OOXML path using a real workbook generated by openpyxl.
    if(app.arguments().size()>1){
        const QString path=app.arguments().at(1);
        QFile source(path);
        const bool opened=source.open(QIODevice::ReadOnly);
        const QByteArray original=opened?source.readAll():QByteArray();
        source.close();
        CHECK(opened&&!original.isEmpty(),"read real XLSX fixture bytes");
        QList<XlsxSheet> sheets;QString error;
        const bool listed=xlsxSheets(path,sheets,error);
        CHECK(listed&&sheets.size()==3&&sheets.value(0).name==QStringLiteral("Đơn hàng")&&
            sheets.value(1).name==QStringLiteral("Danh mục")&&sheets.value(2).name==QStringLiteral("Công thức"),
            "actual XLSX worksheet order and Vietnamese names");
        if(listed&&sheets.size()==3){
            Table orders,lookup;
            const bool ordersOk=readXlsxTable(path,sheets[0],orders,error);
            const bool lookupOk=readXlsxTable(path,sheets[1],lookup,error);
            CHECK(ordersOk&&lookupOk,"read two actual deflated XLSX worksheet parts");
            CHECK(orders.header.value(0)==QStringLiteral("Mã")&&orders.rows.size()==4&&
                orders.rows.value(0).value(0)=="001"&&orders.rows.value(0).value(1)==QStringLiteral("Nguyễn Văn An")&&
                orders.rows.value(2).value(0)=="1","actual workbook preserves Unicode and textual IDs");
            if(ordersOk&&lookupOk){
                const auto joined=joinLookup(orders,lookup,0,0,JoinKind::Left);
                CHECK(joined.ok&&joined.matched==3&&joined.unmatched==1&&
                    joined.table.rows.value(0).value(3)==QStringLiteral("An Khê"),"join two worksheets without coercing ID");
                const auto grouped=groupCsv(orders,0,2);
                CHECK(grouped.ok&&grouped.table.rows.size()==3&&grouped.table.rows.value(0).value(3)=="0.3"&&
                    grouped.table.rows.value(1).value(3)=="12.3456","group actual XLSX raw decimal values");
                FilterSortSpec spec;spec.filterCol=0;spec.value="001";spec.outputColumns={0,2};spec.outputNames={"ID","Amount"};
                const auto filtered=filterSort(orders,spec);
                CHECK(filtered.ok&&filtered.table.rows.size()==2&&filtered.table.header.value(1)=="Amount","filter and project actual XLSX");
                QTemporaryDir temp;Table roundtrip;
                const auto out=temp.filePath("query.csv");
                CHECK(filtered.ok&&writeCsvCopy(out,filtered.table,error)&&readCsv(out,roundtrip,error)&&roundtrip==filtered.table,
                    "export and re-open actual query result CSV");
                const Table previous=orders;
                CHECK(!readXlsxTable(path,sheets[2],orders,error)&&orders==previous,"refuse formula worksheet without altering prior result");
            }
        }
        CHECK(source.open(QIODevice::ReadOnly)&&source.readAll()==original,"real workbook remains byte-identical");
    }

    std::cout << "\nKẾT QUẢ: " << (g_fail ? "CÓ LỖI" : "TẤT CẢ ĐẠT") << "  (đạt " << g_pass << ", lỗi " << g_fail << ")\n";
    return g_fail ? 1 : 0;
}
