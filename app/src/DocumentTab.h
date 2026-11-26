// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "dung/Engine.h"
#include "recovery/RecoveryStore.h"

#include <QHash>
#include <QWidget>
#include <memory>

class QLineEdit;
class QLabel;
class QPushButton;
class QTimer;
class QTabBar;
class QTabWidget;
class QListWidget;
class TileView;

class DocumentTab : public QWidget {
    Q_OBJECT
public:
    DocumentTab(std::unique_ptr<dung::Document> doc, QWidget* parent = nullptr);
    ~DocumentTab() override;

    dung::Document* document() const { return m_doc.get(); }
    dung::DocKind kind() const { return m_doc->kind(); }
    bool readOnly() const { return m_doc->readOnly(); }
    QString title() const;
    QString statusText() const;
    const QHash<QString, QString>& states() const { return m_states; }
    TileView* view() const { return m_view; }

    void runCommand(const QString& uno);
    void copy();
    void cut();
    void paste();
    bool save();            // Ctrl+S: lưu vào tệp mới đã chọn trước đó, hoặc hỏi Lưu thành
    bool saveAs();          // luôn hỏi đường dẫn
    bool exportPdf();
    bool printDocument();   // Ctrl+P: xuất bản in (PDF tạm) → xem trước → in
    bool maybeClose();      // hỏi lưu nếu có thay đổi; false = huỷ đóng
    void showExcelTools(int page);
    void excelAction(const QString& id);  // xl:* — cài đặt trong excel/ExcelUi.cpp
    void setZoom(int percent);
    int zoom() const;
    void adoptRecovered(const recovery::Entry& e);  // bản mở từ phục hồi: xoá bản cũ sau khi đã có bản tự lưu mới / đã lưu
    void setPptNotesMode(bool notes);
    void stepPart(int delta);  // chuyển trang/slide trước (-1) hoặc sau (+1)
    // Bản sao có lề mới (mở trong thẻ mới): nhớ tệp gốc thật để tự lưu/phục hồi và "Lưu thành" không đụng tệp gốc.
    void setCopyOrigin(const QString& originalPath);

signals:
    void titleChanged();
    void statusChanged();
    void commandStateChanged(const QString& cmd, const QString& value);
    void savedTo(const QString& path);
    void templateCopyRequested(const QString& path);
    // Yêu cầu MainWindow mở tệp `patchedPath` (bản sao đã vá) trong thẻ mới; tệp tạm được xoá sau khi tín hiệu trả về.
    void openCopyRequested(const QString& patchedPath, const QString& originalPath, const QString& displayName);

private:
    void onEngineEvent(int type, const QString& payload);
    void rebuildParts();
    void refreshPdfComparison();
    void excelSheetMenu(const QPoint& pos);  // excel/ExcelUi.cpp
    bool doSave(const QString& path, bool confirmedReplace);
    void autosaveTick();
    void dropRecovery();

    std::unique_ptr<dung::Document> m_doc;
    TileView* m_view = nullptr;
    QLineEdit* m_nameBox = nullptr;
    QLineEdit* m_formula = nullptr;
    QTabBar* m_sheets = nullptr;
    QTabWidget* m_excelTools = nullptr;
    QWidget* m_rangeTools = nullptr;
    QWidget* m_formulaAssistant = nullptr;
    unsigned m_pickGeneration = 0;
    QListWidget* m_slides = nullptr;
    QWidget* m_pdfComparison = nullptr;
    QLabel* m_pdfOriginalImage = nullptr;
    QPushButton* m_pdfCompareButton = nullptr;
    int m_pdfComparedPart = -1;
    QString m_savedPath;    // tệp mới mà người dùng đã "Lưu thành" trong phiên này
    QString m_cellAddress;
    bool m_lastMod = false;
    QHash<QString, QString> m_states;
    bool m_pptNotes = false;
    bool m_rebuildingParts = false;
    QTimer* m_autosave = nullptr;
    QString m_recId;                  // mã bản phục hồi của tài liệu này trong phiên
    bool m_dirtySinceAutosave = false;
    bool m_featureLossConfirmed = false;  // đã xác nhận mất tính năng của tệp gốc trong phiên này
    bool m_hasAdopted = false;
    QString m_recOrigin;              // tệp gốc thật của bản đã khôi phục (không phải đường dẫn trong thư mục phục hồi)
    bool m_recOriginSet = false;
    recovery::Entry m_adopted;        // bản mồ côi đã mở (xoá khi an toàn)
};
