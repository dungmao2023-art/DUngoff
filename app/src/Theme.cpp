// SPDX-License-Identifier: MPL-2.0
#include "Theme.h"

#include <QFontDatabase>

namespace theme {

QString iconFontFamily() {
    static const QString fam = [] {
        const QStringList fams = QFontDatabase::families();
        for (const char* f : {"Segoe Fluent Icons", "Segoe MDL2 Assets"})
            if (fams.contains(QString::fromLatin1(f))) return QString::fromLatin1(f);
        return QString();
    }();
    return fam;
}

QString glyph(char16_t mdl2, const QString& fallback) {
    return iconFontFamily().isEmpty() ? fallback : QString(QChar(mdl2));
}

QString styleSheet() {
    return QStringLiteral(R"(
* { color: #1E293B; }
QMainWindow, QWidget#Central { background: #F5F8FC; }

/* Thanh bên trái */
QWidget#Sidebar { background: #1F5FBF; }
QToolButton#SideButton {
  color: white; background: transparent; border: none; border-radius: 8px;
  padding: 6px 2px; font-size: 11px; min-width: 64px; max-width: 64px;
}
QToolButton#SideButton:hover { background: rgba(255,255,255,0.14); }
QToolButton#SideButton:checked { background: rgba(255,255,255,0.24); }
QLabel#SideLogo { color: white; font-weight: 700; font-size: 15px; }

/* Ribbon */
QWidget#Ribbon { background: white; border-bottom: 1px solid #D9E1EC; }
QTabBar#RibbonTabs::tab {
  background: transparent; border: none; padding: 6px 14px; color: #475569; font-size: 13px;
}
QTabBar#RibbonTabs::tab:selected { color: #1F5FBF; border-bottom: 2px solid #1F5FBF; font-weight: 600; }
QTabBar#RibbonTabs::tab:hover { color: #1F5FBF; }
QToolButton#RibbonButton {
  background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 2px 4px;
  font-size: 11px; min-width: 46px;
}
QToolButton#RibbonButton:hover { background: #E8F0FC; border-color: #C9DAF5; }
QToolButton#RibbonButton:checked { background: #D4E3FA; border-color: #9DBCEB; }
QToolButton#RibbonButton:disabled { color: #A0AEC0; }
QLabel#GroupLabel { color: #64748B; font-size: 10px; }
QFrame#GroupSep { color: #E2E8F0; }

/* Thẻ tài liệu */
QTabWidget#DocTabs::pane { border: none; }
QTabBar#DocTabBar::tab {
  background: #E9EFF7; border: 1px solid #D9E1EC; border-bottom: none;
  padding: 5px 12px; margin-right: 2px; border-top-left-radius: 6px; border-top-right-radius: 6px;
  min-width: 120px; max-width: 240px;
}
QTabBar#DocTabBar::tab:selected { background: white; color: #1F5FBF; font-weight: 600; }

/* Thanh công thức */
QWidget#FormulaBar { background: white; border-bottom: 1px solid #D9E1EC; }
QLineEdit { border: 1px solid #D9E1EC; border-radius: 4px; padding: 3px 6px; background: white; }
QLineEdit:focus { border-color: #1F5FBF; }
QLabel#Fx { color: #64748B; font-style: italic; font-weight: 600; }

/* Băng cảnh báo chỉ đọc */
QFrame#Banner { background: #FFF7E0; border-bottom: 1px solid #F5D98B; }
QLabel#BannerText { color: #7A5B00; }

/* Thanh sheet */
QTabBar#SheetTabs::tab {
  background: #EEF2F7; border: 1px solid #D9E1EC; border-top: none; padding: 3px 14px; margin-right: 1px;
}
QTabBar#SheetTabs::tab:selected { background: white; color: #1E8E4F; font-weight: 600; border-bottom: 2px solid #1E8E4F; }

/* Trang chủ */
QWidget#Home { background: #F5F8FC; }
QLabel#HomeTitle { font-size: 26px; font-weight: 700; color: #1F5FBF; }
QLabel#HomeSubtitle { color: #64748B; font-size: 13px; }
QLabel#SectionTitle { font-size: 15px; font-weight: 600; margin-top: 8px; }
QPushButton#Tile {
  background: white; border: 1px solid #D9E1EC; border-radius: 12px; padding: 14px; text-align: left;
  font-size: 13px; min-width: 170px; min-height: 92px;
}
QPushButton#Tile:hover { border-color: #1F5FBF; background: #F8FBFF; }
QPushButton#Primary {
  background: #1F5FBF; color: white; border: none; border-radius: 8px; padding: 8px 18px; font-weight: 600;
}
QPushButton#Primary:hover { background: #16478F; }
QListWidget#Recent { background: white; border: 1px solid #D9E1EC; border-radius: 10px; padding: 4px; }
QListWidget#Recent::item { padding: 8px; border-radius: 6px; }
QListWidget#Recent::item:hover { background: #E8F0FC; }

QStatusBar { background: white; border-top: 1px solid #D9E1EC; color: #475569; }
QStatusBar QLabel { padding: 0 8px; color: #475569; }
)");
}

} // namespace theme
