// SPDX-License-Identifier: MPL-2.0
#include "FormulaCatalog.h"
#include <QHash>
#include <algorithm>

namespace dataassist {

QString categoryName(Category c)
{
    switch (c) {
    case Category::Math: return QStringLiteral("Tính toán");
    case Category::Condition: return QStringLiteral("Điều kiện");
    case Category::Lookup: return QStringLiteral("Tra cứu");
    case Category::Text: return QStringLiteral("Văn bản");
    case Category::Date: return QStringLiteral("Ngày tháng");
    case Category::Finance: return QStringLiteral("Tài chính");
    }
    return {};
}
QList<Category> allCategories()
{
    return {Category::Math, Category::Condition, Category::Lookup, Category::Text, Category::Date, Category::Finance};
}

const QVector<FuncInfo>& catalog()
{
    static const QVector<FuncInfo> c = {
    {QStringLiteral(u"SUM"), Category::Math, QStringLiteral(u"Tôi muốn cộng tổng một vùng số"), QStringLiteral(u"Cộng tất cả số trong vùng"), {{QStringLiteral(u"Vùng cộng"), QStringLiteral(u"A1:A10"), false}}, QStringLiteral(u"=SUM(A1:A10)"), {QStringLiteral(u"tong"), QStringLiteral(u"cong"), QStringLiteral(u"cong tong"), QStringLiteral(u"tinh tong")}, false},
    {QStringLiteral(u"AVERAGE"), Category::Math, QStringLiteral(u"Tôi muốn tính trung bình"), QStringLiteral(u"Trung bình cộng của vùng số"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A1:A10"), false}}, QStringLiteral(u"=AVERAGE(A1:A10)"), {QStringLiteral(u"trung binh"), QStringLiteral(u"binh quan")}, false},
    {QStringLiteral(u"MIN"), Category::Math, QStringLiteral(u"Tôi muốn tìm số nhỏ nhất"), QStringLiteral(u"Giá trị nhỏ nhất trong vùng"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A1:A10"), false}}, QStringLiteral(u"=MIN(A1:A10)"), {QStringLiteral(u"nho nhat"), QStringLiteral(u"thap nhat")}, false},
    {QStringLiteral(u"MAX"), Category::Math, QStringLiteral(u"Tôi muốn tìm số lớn nhất"), QStringLiteral(u"Giá trị lớn nhất trong vùng"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A1:A10"), false}}, QStringLiteral(u"=MAX(A1:A10)"), {QStringLiteral(u"lon nhat"), QStringLiteral(u"cao nhat")}, false},
    {QStringLiteral(u"COUNT"), Category::Math, QStringLiteral(u"Tôi muốn đếm ô có số"), QStringLiteral(u"Đếm số ô chứa số"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A1:A10"), false}}, QStringLiteral(u"=COUNT(A1:A10)"), {QStringLiteral(u"dem"), QStringLiteral(u"dem so")}, false},
    {QStringLiteral(u"COUNTA"), Category::Math, QStringLiteral(u"Tôi muốn đếm ô không trống"), QStringLiteral(u"Đếm số ô có dữ liệu"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A1:A10"), false}}, QStringLiteral(u"=COUNTA(A1:A10)"), {QStringLiteral(u"dem o khong trong"), QStringLiteral(u"dem du lieu")}, false},
    {QStringLiteral(u"ROUND"), Category::Math, QStringLiteral(u"Tôi muốn làm tròn số"), QStringLiteral(u"Làm tròn đến số chữ số thập phân"), {{QStringLiteral(u"Số"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Số chữ số"), QStringLiteral(u"0"), false}}, QStringLiteral(u"=ROUND(A1,0)"), {QStringLiteral(u"lam tron")}, false},
    {QStringLiteral(u"ROUNDUP"), Category::Math, QStringLiteral(u"Tôi muốn làm tròn lên"), QStringLiteral(u"Luôn làm tròn lên"), {{QStringLiteral(u"Số"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Số chữ số"), QStringLiteral(u"0"), false}}, QStringLiteral(u"=ROUNDUP(A1,0)"), {QStringLiteral(u"lam tron len")}, false},
    {QStringLiteral(u"ROUNDDOWN"), Category::Math, QStringLiteral(u"Tôi muốn làm tròn xuống"), QStringLiteral(u"Luôn làm tròn xuống"), {{QStringLiteral(u"Số"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Số chữ số"), QStringLiteral(u"0"), false}}, QStringLiteral(u"=ROUNDDOWN(A1,0)"), {QStringLiteral(u"lam tron xuong")}, false},
    {QStringLiteral(u"ABS"), Category::Math, QStringLiteral(u"Tôi muốn lấy giá trị tuyệt đối"), QStringLiteral(u"Bỏ dấu âm"), {{QStringLiteral(u"Số"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=ABS(A1)"), {QStringLiteral(u"tuyet doi")}, false},
    {QStringLiteral(u"SUMPRODUCT"), Category::Math, QStringLiteral(u"Tôi muốn nhân hai cột rồi cộng"), QStringLiteral(u"Tổng các tích tương ứng (vd. số lượng × đơn giá)"), {{QStringLiteral(u"Vùng 1"), QStringLiteral(u"B2:B10"), false}, {QStringLiteral(u"Vùng 2"), QStringLiteral(u"C2:C10"), false}}, QStringLiteral(u"=SUMPRODUCT(B2:B10,C2:C10)"), {QStringLiteral(u"nhan roi cong"), QStringLiteral(u"thanh tien"), QStringLiteral(u"so luong don gia")}, false},
    {QStringLiteral(u"IF"), Category::Condition, QStringLiteral(u"Tôi muốn chọn kết quả theo điều kiện"), QStringLiteral(u"Nếu đúng thì giá trị 1, sai thì giá trị 2"), {{QStringLiteral(u"Điều kiện"), QStringLiteral(u"A1>=5"), false}, {QStringLiteral(u"Nếu đúng"), QStringLiteral(u"\"Đạt\""), false}, {QStringLiteral(u"Nếu sai"), QStringLiteral(u"\"Chưa đạt\""), false}}, QStringLiteral(u"=IF(A1>=5,\"Đạt\",\"Chưa đạt\")"), {QStringLiteral(u"neu"), QStringLiteral(u"dieu kien"), QStringLiteral(u"dat khong dat")}, false},
    {QStringLiteral(u"IFERROR"), Category::Condition, QStringLiteral(u"Tôi muốn thay lỗi bằng giá trị khác"), QStringLiteral(u"Nếu công thức lỗi thì dùng giá trị thay thế"), {{QStringLiteral(u"Biểu thức"), QStringLiteral(u"A1/B1"), false}, {QStringLiteral(u"Giá trị nếu lỗi"), QStringLiteral(u"0"), false}}, QStringLiteral(u"=IFERROR(A1/B1,0)"), {QStringLiteral(u"loi"), QStringLiteral(u"bao loi"), QStringLiteral(u"an loi")}, false},
    {QStringLiteral(u"AND"), Category::Condition, QStringLiteral(u"Tôi muốn tất cả điều kiện cùng đúng"), QStringLiteral(u"Đúng khi mọi điều kiện đúng"), {{QStringLiteral(u"Điều kiện 1"), QStringLiteral(u"A1>0"), false}, {QStringLiteral(u"Điều kiện 2"), QStringLiteral(u"B1>0"), false}}, QStringLiteral(u"=AND(A1>0,B1>0)"), {QStringLiteral(u"va"), QStringLiteral(u"dong thoi")}, false},
    {QStringLiteral(u"OR"), Category::Condition, QStringLiteral(u"Tôi muốn một trong các điều kiện đúng"), QStringLiteral(u"Đúng khi có ít nhất một điều kiện đúng"), {{QStringLiteral(u"Điều kiện 1"), QStringLiteral(u"A1>0"), false}, {QStringLiteral(u"Điều kiện 2"), QStringLiteral(u"B1>0"), false}}, QStringLiteral(u"=OR(A1>0,B1>0)"), {QStringLiteral(u"hoac")}, false},
    {QStringLiteral(u"SUMIF"), Category::Condition, QStringLiteral(u"Tôi muốn cộng tổng có điều kiện"), QStringLiteral(u"Cộng các ô thoả một điều kiện"), {{QStringLiteral(u"Vùng điều kiện"), QStringLiteral(u"A2:A10"), false}, {QStringLiteral(u"Điều kiện"), QStringLiteral(u"\"Hà Nội\""), false}, {QStringLiteral(u"Vùng cộng"), QStringLiteral(u"B2:B10"), true}}, QStringLiteral(u"=SUMIF(A2:A10,\"Hà Nội\",B2:B10)"), {QStringLiteral(u"tong co dieu kien"), QStringLiteral(u"cong theo dieu kien")}, false},
    {QStringLiteral(u"SUMIFS"), Category::Condition, QStringLiteral(u"Tôi muốn cộng tổng nhiều điều kiện"), QStringLiteral(u"Cộng các ô thoả nhiều điều kiện"), {{QStringLiteral(u"Vùng cộng"), QStringLiteral(u"C2:C10"), false}, {QStringLiteral(u"Vùng điều kiện 1"), QStringLiteral(u"A2:A10"), false}, {QStringLiteral(u"Điều kiện 1"), QStringLiteral(u"\"Hà Nội\""), false}, {QStringLiteral(u"Vùng điều kiện 2"), QStringLiteral(u"B2:B10"), false}, {QStringLiteral(u"Điều kiện 2"), QStringLiteral(u"\">5\""), false}}, QStringLiteral(u"=SUMIFS(C2:C10,A2:A10,\"Hà Nội\",B2:B10,\">5\")"), {QStringLiteral(u"tong nhieu dieu kien")}, false},
    {QStringLiteral(u"COUNTIF"), Category::Condition, QStringLiteral(u"Tôi muốn đếm ô theo điều kiện"), QStringLiteral(u"Đếm số ô thoả điều kiện"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A2:A10"), false}, {QStringLiteral(u"Điều kiện"), QStringLiteral(u"\"Hà Nội\""), false}}, QStringLiteral(u"=COUNTIF(A2:A10,\"Hà Nội\")"), {QStringLiteral(u"dem co dieu kien"), QStringLiteral(u"dem theo dieu kien")}, false},
    {QStringLiteral(u"COUNTIFS"), Category::Condition, QStringLiteral(u"Tôi muốn đếm theo nhiều điều kiện"), QStringLiteral(u"Đếm ô thoả nhiều điều kiện"), {{QStringLiteral(u"Vùng 1"), QStringLiteral(u"A2:A10"), false}, {QStringLiteral(u"Điều kiện 1"), QStringLiteral(u"\"Hà Nội\""), false}, {QStringLiteral(u"Vùng 2"), QStringLiteral(u"B2:B10"), false}, {QStringLiteral(u"Điều kiện 2"), QStringLiteral(u"\">5\""), false}}, QStringLiteral(u"=COUNTIFS(A2:A10,\"Hà Nội\",B2:B10,\">5\")"), {QStringLiteral(u"dem nhieu dieu kien")}, false},
    {QStringLiteral(u"VLOOKUP"), Category::Lookup, QStringLiteral(u"Tôi muốn tìm theo mã trong bảng (cột đầu)"), QStringLiteral(u"Tìm giá trị ở cột đầu rồi lấy cột thứ n"), {{QStringLiteral(u"Giá trị cần tìm"), QStringLiteral(u"A2"), false}, {QStringLiteral(u"Bảng"), QStringLiteral(u"F2:H100"), false}, {QStringLiteral(u"Cột trả về"), QStringLiteral(u"2"), false}, {QStringLiteral(u"Khớp chính xác"), QStringLiteral(u"FALSE"), false}}, QStringLiteral(u"=VLOOKUP(A2,F2:H100,2,FALSE)"), {QStringLiteral(u"tim theo ma"), QStringLiteral(u"tra cuu"), QStringLiteral(u"vlookup"), QStringLiteral(u"tim trong bang")}, false},
    {QStringLiteral(u"HLOOKUP"), Category::Lookup, QStringLiteral(u"Tôi muốn tìm theo hàng đầu"), QStringLiteral(u"Tìm ở hàng đầu rồi lấy hàng thứ n"), {{QStringLiteral(u"Giá trị"), QStringLiteral(u"A2"), false}, {QStringLiteral(u"Bảng"), QStringLiteral(u"F2:K10"), false}, {QStringLiteral(u"Hàng"), QStringLiteral(u"2"), false}, {QStringLiteral(u"Khớp chính xác"), QStringLiteral(u"FALSE"), false}}, QStringLiteral(u"=HLOOKUP(A2,F2:K10,2,FALSE)"), {QStringLiteral(u"tim theo hang")}, false},
    {QStringLiteral(u"INDEX"), Category::Lookup, QStringLiteral(u"Tôi muốn lấy ô theo vị trí"), QStringLiteral(u"Lấy giá trị theo số hàng/cột"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A1:C10"), false}, {QStringLiteral(u"Hàng"), QStringLiteral(u"2"), false}, {QStringLiteral(u"Cột"), QStringLiteral(u"1"), true}}, QStringLiteral(u"=INDEX(A1:C10,2,1)"), {QStringLiteral(u"lay o theo vi tri")}, false},
    {QStringLiteral(u"MATCH"), Category::Lookup, QStringLiteral(u"Tôi muốn tìm vị trí của giá trị"), QStringLiteral(u"Trả số thứ tự của giá trị trong vùng"), {{QStringLiteral(u"Giá trị"), QStringLiteral(u"A2"), false}, {QStringLiteral(u"Vùng"), QStringLiteral(u"F2:F100"), false}, {QStringLiteral(u"Kiểu khớp"), QStringLiteral(u"0"), false}}, QStringLiteral(u"=MATCH(A2,F2:F100,0)"), {QStringLiteral(u"vi tri"), QStringLiteral(u"thu tu")}, false},
    {QStringLiteral(u"XLOOKUP"), Category::Lookup, QStringLiteral(u"Tôi muốn tìm và trả về (thay VLOOKUP)"), QStringLiteral(u"Tìm ở một cột, trả về từ cột khác"), {{QStringLiteral(u"Giá trị"), QStringLiteral(u"A2"), false}, {QStringLiteral(u"Vùng tìm"), QStringLiteral(u"F2:F100"), false}, {QStringLiteral(u"Vùng trả về"), QStringLiteral(u"G2:G100"), false}}, QStringLiteral(u"=XLOOKUP(A2,F2:F100,G2:G100)"), {QStringLiteral(u"tim kiem moi"), QStringLiteral(u"xlookup")}, true},
    {QStringLiteral(u"FILTER"), Category::Lookup, QStringLiteral(u"Tôi muốn lọc các dòng theo điều kiện"), QStringLiteral(u"Trả về các dòng thoả điều kiện (hàm mảng động)"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A2:C100"), false}, {QStringLiteral(u"Điều kiện"), QStringLiteral(u"B2:B100>5"), false}}, QStringLiteral(u"=FILTER(A2:C100,B2:B100>5)"), {QStringLiteral(u"loc dong"), QStringLiteral(u"loc du lieu")}, true},
    {QStringLiteral(u"SORT"), Category::Lookup, QStringLiteral(u"Tôi muốn sắp xếp một vùng"), QStringLiteral(u"Trả về vùng đã sắp xếp (hàm mảng động)"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A2:C100"), false}, {QStringLiteral(u"Cột"), QStringLiteral(u"1"), false}, {QStringLiteral(u"Thứ tự"), QStringLiteral(u"1"), false}}, QStringLiteral(u"=SORT(A2:C100,1,1)"), {QStringLiteral(u"sap xep")}, true},
    {QStringLiteral(u"UNIQUE"), Category::Lookup, QStringLiteral(u"Tôi muốn lấy danh sách không trùng"), QStringLiteral(u"Bỏ giá trị lặp (hàm mảng động)"), {{QStringLiteral(u"Vùng"), QStringLiteral(u"A2:A100"), false}}, QStringLiteral(u"=UNIQUE(A2:A100)"), {QStringLiteral(u"khong trung"), QStringLiteral(u"loai trung"), QStringLiteral(u"duy nhat")}, true},
    {QStringLiteral(u"CONCATENATE"), Category::Text, QStringLiteral(u"Tôi muốn nối nhiều ô thành chuỗi"), QStringLiteral(u"Nối văn bản"), {{QStringLiteral(u"Chuỗi 1"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Chuỗi 2"), QStringLiteral(u"\" \""), false}, {QStringLiteral(u"Chuỗi 3"), QStringLiteral(u"B1"), false}}, QStringLiteral(u"=CONCATENATE(A1,\" \",B1)"), {QStringLiteral(u"noi chuoi"), QStringLiteral(u"ghep ten"), QStringLiteral(u"noi")}, false},
    {QStringLiteral(u"LEFT"), Category::Text, QStringLiteral(u"Tôi muốn lấy ký tự bên trái"), QStringLiteral(u"Lấy n ký tự đầu"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Số ký tự"), QStringLiteral(u"3"), false}}, QStringLiteral(u"=LEFT(A1,3)"), {QStringLiteral(u"lay ben trai"), QStringLiteral(u"ky tu dau")}, false},
    {QStringLiteral(u"RIGHT"), Category::Text, QStringLiteral(u"Tôi muốn lấy ký tự bên phải"), QStringLiteral(u"Lấy n ký tự cuối"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Số ký tự"), QStringLiteral(u"3"), false}}, QStringLiteral(u"=RIGHT(A1,3)"), {QStringLiteral(u"lay ben phai"), QStringLiteral(u"ky tu cuoi")}, false},
    {QStringLiteral(u"MID"), Category::Text, QStringLiteral(u"Tôi muốn lấy ký tự ở giữa"), QStringLiteral(u"Lấy n ký tự từ vị trí bắt đầu"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Vị trí"), QStringLiteral(u"2"), false}, {QStringLiteral(u"Số ký tự"), QStringLiteral(u"3"), false}}, QStringLiteral(u"=MID(A1,2,3)"), {QStringLiteral(u"lay o giua"), QStringLiteral(u"cat chuoi")}, false},
    {QStringLiteral(u"LEN"), Category::Text, QStringLiteral(u"Tôi muốn đếm độ dài chuỗi"), QStringLiteral(u"Số ký tự"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=LEN(A1)"), {QStringLiteral(u"do dai"), QStringLiteral(u"dem ky tu")}, false},
    {QStringLiteral(u"TRIM"), Category::Text, QStringLiteral(u"Tôi muốn xoá khoảng trắng thừa"), QStringLiteral(u"Bỏ khoảng trắng đầu/cuối/lặp"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=TRIM(A1)"), {QStringLiteral(u"xoa khoang trang"), QStringLiteral(u"cat khoang trang")}, false},
    {QStringLiteral(u"UPPER"), Category::Text, QStringLiteral(u"Tôi muốn chuyển thành CHỮ HOA"), QStringLiteral(u"Viết hoa toàn bộ"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=UPPER(A1)"), {QStringLiteral(u"chu hoa"), QStringLiteral(u"viet hoa")}, false},
    {QStringLiteral(u"LOWER"), Category::Text, QStringLiteral(u"Tôi muốn chuyển thành chữ thường"), QStringLiteral(u"Viết thường toàn bộ"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=LOWER(A1)"), {QStringLiteral(u"chu thuong"), QStringLiteral(u"viet thuong")}, false},
    {QStringLiteral(u"PROPER"), Category::Text, QStringLiteral(u"Tôi muốn viết Hoa Chữ Cái Đầu"), QStringLiteral(u"Hoa chữ cái đầu mỗi từ"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=PROPER(A1)"), {QStringLiteral(u"hoa chu cai dau"), QStringLiteral(u"viet hoa ten")}, false},
    {QStringLiteral(u"TEXT"), Category::Text, QStringLiteral(u"Tôi muốn định dạng số/ngày thành chữ"), QStringLiteral(u"Ép định dạng hiển thị"), {{QStringLiteral(u"Giá trị"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Định dạng"), QStringLiteral(u"\"dd/mm/yyyy\""), false}}, QStringLiteral(u"=TEXT(A1,\"dd/mm/yyyy\")"), {QStringLiteral(u"dinh dang"), QStringLiteral(u"so thanh chu"), QStringLiteral(u"ngay thanh chu")}, false},
    {QStringLiteral(u"SUBSTITUTE"), Category::Text, QStringLiteral(u"Tôi muốn thay đoạn chữ"), QStringLiteral(u"Thay chuỗi cũ bằng chuỗi mới"), {{QStringLiteral(u"Chuỗi"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Cũ"), QStringLiteral(u"\"a\""), false}, {QStringLiteral(u"Mới"), QStringLiteral(u"\"b\""), false}}, QStringLiteral(u"=SUBSTITUTE(A1,\"a\",\"b\")"), {QStringLiteral(u"thay the"), QStringLiteral(u"doi chu")}, false},
    {QStringLiteral(u"TODAY"), Category::Date, QStringLiteral(u"Tôi muốn ngày hôm nay"), QStringLiteral(u"Ngày hiện tại"), {}, QStringLiteral(u"=TODAY()"), {QStringLiteral(u"hom nay"), QStringLiteral(u"ngay hien tai")}, false},
    {QStringLiteral(u"NOW"), Category::Date, QStringLiteral(u"Tôi muốn ngày giờ hiện tại"), QStringLiteral(u"Ngày và giờ hiện tại"), {}, QStringLiteral(u"=NOW()"), {QStringLiteral(u"bay gio"), QStringLiteral(u"gio hien tai")}, false},
    {QStringLiteral(u"DATE"), Category::Date, QStringLiteral(u"Tôi muốn tạo ngày từ năm, tháng, ngày"), QStringLiteral(u"Ghép ngày"), {{QStringLiteral(u"Năm"), QStringLiteral(u"2026"), false}, {QStringLiteral(u"Tháng"), QStringLiteral(u"1"), false}, {QStringLiteral(u"Ngày"), QStringLiteral(u"15"), false}}, QStringLiteral(u"=DATE(2026,1,15)"), {QStringLiteral(u"tao ngay"), QStringLiteral(u"ghep ngay")}, false},
    {QStringLiteral(u"YEAR"), Category::Date, QStringLiteral(u"Tôi muốn lấy năm từ ngày"), QStringLiteral(u"Năm của ngày"), {{QStringLiteral(u"Ngày"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=YEAR(A1)"), {QStringLiteral(u"lay nam")}, false},
    {QStringLiteral(u"MONTH"), Category::Date, QStringLiteral(u"Tôi muốn lấy tháng từ ngày"), QStringLiteral(u"Tháng của ngày"), {{QStringLiteral(u"Ngày"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=MONTH(A1)"), {QStringLiteral(u"lay thang")}, false},
    {QStringLiteral(u"DAY"), Category::Date, QStringLiteral(u"Tôi muốn lấy ngày trong tháng"), QStringLiteral(u"Ngày của ngày"), {{QStringLiteral(u"Ngày"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=DAY(A1)"), {QStringLiteral(u"lay ngay")}, false},
    {QStringLiteral(u"DATEDIF"), Category::Date, QStringLiteral(u"Tôi muốn tính khoảng cách giữa hai ngày"), QStringLiteral(u"Số ngày/tháng/năm giữa hai ngày"), {{QStringLiteral(u"Từ ngày"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Đến ngày"), QStringLiteral(u"B1"), false}, {QStringLiteral(u"Đơn vị"), QStringLiteral(u"\"D\""), false}}, QStringLiteral(u"=DATEDIF(A1,B1,\"D\")"), {QStringLiteral(u"so ngay"), QStringLiteral(u"khoang cach ngay"), QStringLiteral(u"tuoi"), QStringLiteral(u"tham nien")}, false},
    {QStringLiteral(u"NETWORKDAYS"), Category::Date, QStringLiteral(u"Tôi muốn đếm ngày làm việc"), QStringLiteral(u"Số ngày làm việc giữa hai ngày (trừ T7, CN)"), {{QStringLiteral(u"Từ ngày"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Đến ngày"), QStringLiteral(u"B1"), false}}, QStringLiteral(u"=NETWORKDAYS(A1,B1)"), {QStringLiteral(u"ngay lam viec")}, false},
    {QStringLiteral(u"PMT"), Category::Finance, QStringLiteral(u"Tôi muốn tính tiền trả mỗi kỳ (vay)"), QStringLiteral(u"Số tiền phải trả đều mỗi kỳ"), {{QStringLiteral(u"Lãi suất kỳ"), QStringLiteral(u"0.01"), false}, {QStringLiteral(u"Số kỳ"), QStringLiteral(u"12"), false}, {QStringLiteral(u"Giá trị hiện tại"), QStringLiteral(u"-100000000"), false}}, QStringLiteral(u"=PMT(0.01,12,-100000000)"), {QStringLiteral(u"tra gop"), QStringLiteral(u"vay"), QStringLiteral(u"tien tra moi ky")}, false},
    {QStringLiteral(u"NPV"), Category::Finance, QStringLiteral(u"Tôi muốn giá trị hiện tại ròng"), QStringLiteral(u"Chiết khấu dòng tiền"), {{QStringLiteral(u"Lãi suất"), QStringLiteral(u"0.1"), false}, {QStringLiteral(u"Dòng tiền"), QStringLiteral(u"B1:B5"), false}}, QStringLiteral(u"=NPV(0.1,B1:B5)"), {QStringLiteral(u"hien tai rong"), QStringLiteral(u"chiet khau")}, false},
    {QStringLiteral(u"IRR"), Category::Finance, QStringLiteral(u"Tôi muốn tỷ suất hoàn vốn nội bộ"), QStringLiteral(u"IRR của dòng tiền"), {{QStringLiteral(u"Dòng tiền"), QStringLiteral(u"B1:B6"), false}}, QStringLiteral(u"=IRR(B1:B6)"), {QStringLiteral(u"hoan von"), QStringLiteral(u"noi bo")}, false},
    {QStringLiteral(u"COUNTBLANK"), Category::Math, QStringLiteral(u"Tôi muốn đếm ô trống"), QStringLiteral(u"Đếm số ô không có nội dung"), {{QStringLiteral(u"Vùng dữ liệu"), QStringLiteral(u"A2:A10"), false}}, QStringLiteral(u"=COUNTBLANK(A2:A10)"), {QStringLiteral(u"dem o trong")}, false},
    {QStringLiteral(u"PRODUCT"), Category::Math, QStringLiteral(u"Tôi muốn nhân tất cả các số"), QStringLiteral(u"Tích của các số trong vùng"), {{QStringLiteral(u"Vùng số"), QStringLiteral(u"A2:A10"), false}}, QStringLiteral(u"=PRODUCT(A2:A10)"), {QStringLiteral(u"nhan tat ca")}, false},
    {QStringLiteral(u"AVERAGEIF"), Category::Condition, QStringLiteral(u"Tôi muốn tính trung bình theo điều kiện"), QStringLiteral(u"Tính trung bình các ô thỏa một điều kiện"), {{QStringLiteral(u"Vùng kiểm tra"), QStringLiteral(u"A2:A10"), false}, {QStringLiteral(u"Điều kiện"), QStringLiteral(u"\">0\""), false}, {QStringLiteral(u"Vùng tính trung bình"), QStringLiteral(u"B2:B10"), true}}, QStringLiteral(u"=AVERAGEIF(A2:A10,\">0\",B2:B10)"), {QStringLiteral(u"trung binh theo dieu kien")}, false},
    {QStringLiteral(u"AVERAGEIFS"), Category::Condition, QStringLiteral(u"Tôi muốn tính trung bình nhiều điều kiện"), QStringLiteral(u"Tính trung bình theo các cặp vùng và điều kiện"), {{QStringLiteral(u"Vùng tính trung bình"), QStringLiteral(u"B2:B10"), false}, {QStringLiteral(u"Vùng điều kiện 1"), QStringLiteral(u"A2:A10"), false}, {QStringLiteral(u"Điều kiện 1"), QStringLiteral(u"\">0\""), false}, {QStringLiteral(u"Vùng điều kiện 2"), QStringLiteral(u"C2:C10"), false}, {QStringLiteral(u"Điều kiện 2"), QStringLiteral(u"\"A\""), false}}, QStringLiteral(u"=AVERAGEIFS(B2:B10,A2:A10,\">0\",C2:C10,\"A\")"), {QStringLiteral(u"trung binh nhieu dieu kien")}, false},
    {QStringLiteral(u"NOT"), Category::Condition, QStringLiteral(u"Tôi muốn đảo ngược điều kiện"), QStringLiteral(u"Trả kết quả logic ngược lại"), {{QStringLiteral(u"Điều kiện"), QStringLiteral(u"A1>0"), false}}, QStringLiteral(u"=NOT(A1>0)"), {QStringLiteral(u"dao nguoc dieu kien")}, false},
    {QStringLiteral(u"LOOKUP"), Category::Lookup, QStringLiteral(u"Tôi muốn tìm giá trị gần đúng"), QStringLiteral(u"Tìm trong vùng đã sắp xếp tăng dần"), {{QStringLiteral(u"Giá trị cần tìm"), QStringLiteral(u"A2"), false}, {QStringLiteral(u"Vùng tìm kiếm"), QStringLiteral(u"F2:F10"), false}, {QStringLiteral(u"Vùng trả kết quả"), QStringLiteral(u"G2:G10"), true}}, QStringLiteral(u"=LOOKUP(A2,F2:F10,G2:G10)"), {QStringLiteral(u"tra cuu gan dung")}, false},
    {QStringLiteral(u"SQRT"), Category::Math, QStringLiteral(u"Tôi muốn tính căn bậc hai"), QStringLiteral(u"Căn bậc hai của số không âm"), {{QStringLiteral(u"Số"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=SQRT(A1)"), {QStringLiteral(u"can bac hai")}, false},
    {QStringLiteral(u"INT"), Category::Math, QStringLiteral(u"Tôi muốn làm tròn xuống số nguyên"), QStringLiteral(u"Làm tròn xuống tới số nguyên gần nhất"), {{QStringLiteral(u"Số"), QStringLiteral(u"A1"), false}}, QStringLiteral(u"=INT(A1)"), {QStringLiteral(u"phan nguyen")}, false},
    {QStringLiteral(u"MOD"), Category::Math, QStringLiteral(u"Tôi muốn chia lấy số dư"), QStringLiteral(u"Số dư sau phép chia"), {{QStringLiteral(u"Số bị chia"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Số chia"), QStringLiteral(u"2"), false}}, QStringLiteral(u"=MOD(A1,2)"), {QStringLiteral(u"so du")}, false},
    {QStringLiteral(u"POWER"), Category::Math, QStringLiteral(u"Tôi muốn tính lũy thừa"), QStringLiteral(u"Nâng cơ số lên số mũ"), {{QStringLiteral(u"Cơ số"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Số mũ"), QStringLiteral(u"2"), false}}, QStringLiteral(u"=POWER(A1,2)"), {QStringLiteral(u"luy thua")}, false},
    {QStringLiteral(u"CONCAT"), Category::Text, QStringLiteral(u"Tôi muốn nối văn bản"), QStringLiteral(u"Nối các chuỗi hoặc vùng ô"), {{QStringLiteral(u"Chuỗi hoặc vùng 1"), QStringLiteral(u"A1"), false}, {QStringLiteral(u"Chuỗi hoặc vùng 2"), QStringLiteral(u"B1"), true}}, QStringLiteral(u"=CONCAT(A1,B1)"), {QStringLiteral(u"noi van ban")}, false},
    };
    return c;
}

const FuncInfo* findFunction(const QString &name)
{
    for (const FuncInfo &f : catalog())
        if (f.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0) return &f;
    return nullptr;
}
QVector<const FuncInfo*> byCategory(Category cat)
{
    QVector<const FuncInfo*> r;
    for (const FuncInfo &f : catalog()) if (f.cat == cat) r.push_back(&f);
    return r;
}

QString foldVietnamese(const QString &s)
{
    QString d = s.normalized(QString::NormalizationForm_D);
    QString r;
    for (QChar ch : d) {
        if (ch.category() == QChar::Mark_NonSpacing) continue;
        if (ch == QChar(0x0111)) r += QLatin1Char('d');
        else if (ch == QChar(0x0110)) r += QLatin1Char('d');
        else r += ch.toLower();
    }
    return r.simplified();
}

QVector<const FuncInfo*> search(const QString &query, int maxResults)
{
    const QString q = foldVietnamese(query);
    QVector<QPair<int, const FuncInfo*>> scored;
    if (q.isEmpty()) return {};
    const QStringList words = q.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const FuncInfo &f : catalog()) {
        int score = 0;
        const QString name = f.name.toLower();
        const QString intent = foldVietnamese(f.intent);
        const QString desc = foldVietnamese(f.description);
        if (name == q) score += 100;
        else if (name.startsWith(q)) score += 40;
        for (const QString &k : f.keywords) {
            const QString fk = foldVietnamese(k);
            if (fk == q) score += 60;
            else if (fk.contains(q)) score += 30;
        }
        if (intent.contains(q)) score += 25;
        for (const QString &w : words) {
            if (w.size() < 2) continue;
            if (name == w) score += 20;
            for (const QString &k : f.keywords) if (foldVietnamese(k).contains(w)) { score += 6; break; }
            if (intent.contains(w)) score += 4;
            if (desc.contains(w)) score += 2;
        }
        if (score > 0) scored.push_back({score, &f});
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    QVector<const FuncInfo*> r;
    for (const auto &p : scored) { if (r.size() >= maxResults) break; r.push_back(p.second); }
    return r;
}

bool hasConditionPairs(const FuncInfo& f) {
    return f.name == QLatin1String("SUMIFS") || f.name == QLatin1String("COUNTIFS") || f.name == QLatin1String("AVERAGEIFS");
}
QVector<ArgInfo> conditionArgs(const FuncInfo& f, int pairs) {
    if (!hasConditionPairs(f)) return f.args;
    QVector<ArgInfo> args;
    if (f.name != QLatin1String("COUNTIFS")) args << f.args.first();
    for (int n = 1; n <= qBound(1, pairs, 127); ++n) {
        args << ArgInfo{QStringLiteral("Vùng điều kiện %1").arg(n), QStringLiteral("A2:A10"), false}
             << ArgInfo{QStringLiteral("Điều kiện %1").arg(n), QStringLiteral("\">5\""), false};
    }
    return args;
}
BuildResult buildFormula(const FuncInfo &f, const QStringList &argValues, QChar sep)
{
    QVector<ArgInfo> args = f.args;
    if (hasConditionPairs(f)) {
        const int prefix = f.name == QLatin1String("COUNTIFS") ? 0 : 1;
        const int count = int(argValues.size()) - prefix;
        if (count < 2 || count % 2 || count > 254)
            return {false, {}, QStringLiteral("Cần từ 1 đến 127 cặp vùng/điều kiện đầy đủ.")};
        args = conditionArgs(f, count / 2);
    } else if (argValues.size() > args.size()) {
        return {false, {}, QStringLiteral("Có quá nhiều đối số.")};
    }
    BuildResult r;
    QStringList used;
    int lastFilled = -1;
    for (int i = 0; i < args.size(); ++i)
        if (i < argValues.size() && !argValues[i].trimmed().isEmpty()) lastFilled = i;
    for (int i = 0; i <= lastFilled; ++i) {
        const QString v = i < argValues.size() ? argValues[i].trimmed() : QString();
        if (v.isEmpty()) {
            r.error = QStringLiteral("Thiếu đối số \"%1\"%2").arg(args[i].name, args[i].optional ? QStringLiteral(" (không thể bỏ trống khi đối số sau đã nhập)") : QString());
            return r;
        }
        used << v;
    }
    for (int i = lastFilled + 1; i < args.size(); ++i)
        if (!args[i].optional) { r.error = QStringLiteral("Thiếu đối số \"%1\"").arg(args[i].name); return r; }
    r.ok = true;
    r.formula = QLatin1Char('=') + f.name + QLatin1Char('(') + used.join(sep) + QLatin1Char(')');
    return r;
}

SyntaxCheck quickSyntaxCheck(const QString &formula)
{
    SyntaxCheck r;
    const QString f = formula.trimmed();
    if (!f.startsWith(QLatin1Char('='))) { r.ok = false; r.message = QStringLiteral("Công thức phải bắt đầu bằng dấu ="); return r; }
    int depth = 0; bool inStr = false;
    for (int i = 1; i < f.size(); ++i) {
        const QChar c = f[i];
        if (c == QLatin1Char('"')) { inStr = !inStr; continue; }
        if (inStr) continue;
        if (c == QLatin1Char('(')) ++depth;
        else if (c == QLatin1Char(')')) { if (--depth < 0) { r.ok = false; r.message = QStringLiteral("Thừa dấu đóng ngoặc ) ở vị trí %1").arg(i + 1); return r; } }
    }
    if (inStr) { r.ok = false; r.message = QStringLiteral("Thiếu dấu nháy đóng \""); return r; }
    if (depth > 0) { r.ok = false; r.message = QStringLiteral("Thiếu %1 dấu đóng ngoặc )").arg(depth); return r; }
    if (f.size() == 1) { r.ok = false; r.message = QStringLiteral("Công thức trống"); return r; }
    return r;
}

QString replacementHint(const QString &name)
{
    const QString n = name.toUpper();
    if (n == QLatin1String("XLOOKUP")) return QStringLiteral(u"Cần engine mới. Thay bằng =INDEX(vùng_trả_về;MATCH(giá_trị;vùng_tìm;0)) hoặc VLOOKUP.");
    if (n == QLatin1String("FILTER")) return QStringLiteral(u"Cần engine mới. Thay bằng bộ lọc tự động (Dữ liệu ▸ Lọc) hoặc SUMIF/COUNTIF/AVERAGEIF cho phép tính theo điều kiện.");
    if (n == QLatin1String("SORT")) return QStringLiteral(u"Cần engine mới. Thay bằng nút Sắp xếp A→Z / Z→A của thanh công cụ (sắp xếp tại chỗ).");
    if (n == QLatin1String("UNIQUE")) return QStringLiteral(u"Cần engine mới. Thay bằng công cụ “Tìm dòng trùng” trong Công cụ vùng dữ liệu, hoặc bộ lọc nâng cao bỏ bản sao.");
    return {};
}

}  // namespace dataassist
