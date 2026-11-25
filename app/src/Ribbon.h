// SPDX-License-Identifier: MPL-2.0
// Ribbon theo ngữ cảnh: mỗi loại tài liệu có bộ tab/nút riêng. Nút có id bắt đầu ".uno:" là
// lệnh gửi thẳng tới engine; id khác là hành động của ứng dụng (lưu, xuất PDF, zoom...).
#pragma once
#include "dung/FormatPolicy.h"

#include <QHash>
#include <QStringList>
#include <QPointer>
#include <QWidget>

class QTabBar;
class QStackedWidget;
class QToolButton;

class Ribbon : public QWidget {
    Q_OBJECT
public:
    explicit Ribbon(QWidget* parent = nullptr);
    void setKind(dung::DocKind kind, bool readOnly);
    void setCommandState(const QString& cmd, const QString& value);
    void resetStates(const QHash<QString, QString>& states);
    QStringList commandIds() const;  // mọi id nút trên mọi ribbon (dùng cho kiểm thử icon)

signals:
    void unoCommand(const QString& cmd);
    void appAction(const QString& id);

private:
    struct Btn { QString id; QString label; char16_t mdl2; QString fallback; bool checkable; };
    struct Group { QString title; QList<Btn> buttons; };
    struct Tab { QString title; QList<Group> groups; };
    QList<Tab> tabsFor(dung::DocKind kind, bool readOnly) const;
    QWidget* buildPage(const Tab& t);
    QList<Tab> wordTabs(const Group& file, const Group& undo, const Group& clip, const Group& view) const;  // app/src/word/WordRibbon.cpp

    QTabBar* m_tabs;
    QStackedWidget* m_pages;
    QHash<QString, QPointer<QToolButton>> m_buttons;
    int m_lastKind = -1;
    bool m_lastReadOnly = false;
};
