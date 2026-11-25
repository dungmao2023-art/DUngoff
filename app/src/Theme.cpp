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

QColor accentFor(const QString& kind) {
    if (kind == QLatin1String("excel")) return kExcel;
    if (kind == QLatin1String("ppt")) return kPowerPoint;
    if (kind == QLatin1String("pdf")) return kPdf;
    return kWord;
}

QString styleSheet() {
    QString css = QStringLiteral(R"(
* { color: #17294D; }
QMainWindow, QWidget#Central { background: #F4F8FF; }
QScrollArea { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #C9D5EA; border-radius: 4px; min-height: 28px; }
QScrollBar::handle:vertical:hover { background: #A9BBDC; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #C9D5EA; border-radius: 4px; min-width: 28px; }
QScrollBar::handle:horizontal:hover { background: #A9BBDC; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; border: none; background: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QToolTip { background: #17294D; color: white; border: none; padding: 4px 8px; }

/* ===== Thanh bên ===== */
QWidget#Sidebar { background: #FBFCFF; border-right: 1px solid #DCE5F2; }
QWidget#SideScrollBody, QScrollArea#SideScroll { background: transparent; border: none; }
QLabel#SideBrand { font-size: 17px; font-weight: 700; color: #17294D; }
QLabel#SideSub { font-size: 9px; font-weight: 700; color: #8A97B4; letter-spacing: 1px; }
QLabel#SideSection { font-size: 10px; font-weight: 700; color: #8A97B4; letter-spacing: 1px; padding: 14px 8px 4px 8px; }
QPushButton#NavItem {
  background: transparent; border: none; border-left: 3px solid transparent; border-radius: 10px;
  text-align: left; padding: 3px 8px; font-size: 13px; color: #4A5A7C; min-height: 34px;
}
QPushButton#NavItem:hover { background: #EEF4FF; }
QPushButton#NavItem:checked { background: #E8F0FF; border-left: 3px solid #2864BA; color: #17294D; font-weight: 700; }
QFrame#PrivacyCard { background: #F0F6FF; border: 1px solid #DCE5F2; border-radius: 12px; }
QFrame#PrivacyCard QLabel { color: #4A5A7C; font-size: 11px; }
QWidget#Sidebar[compact="true"] QPushButton#NavItem { padding: 6px 6px; }

/* ===== Thanh trên ===== */
QWidget#TopBar { background: white; }
QWidget#TopRow { background: white; border-bottom: 1px solid #E6EDF7; }
QPushButton#TopTab {
  background: transparent; border: none; border-bottom: 3px solid transparent; border-radius: 0;
  padding: 0 14px; font-size: 13px; font-weight: 600; color: #6B7A99;
}
QPushButton#TopTab:hover { color: #17294D; }
QPushButton#TopTab[active="true"] { color: #2864BA; border-bottom: 3px solid #2864BA; }
QPushButton#TopTab[active="true"][kind="excel"] { color: #16804E; border-bottom: 3px solid #16804E; }
QPushButton#TopTab[active="true"][kind="ppt"] { color: #C45530; border-bottom: 3px solid #C45530; }
QPushButton#TopTab[active="true"][kind="pdf"] { color: #B94449; border-bottom: 3px solid #B94449; }
QPushButton#RoundBtn { background: white; border: 1px solid #DCE5F2; border-radius: 18px; }
QPushButton#RoundBtn:hover { background: #EEF4FF; border-color: #BFD3F2; }
QLabel#TopSave { color: #6B7A99; font-size: 11px; }
QLabel#Avatar { background: #E3EDFF; color: #2864BA; border-radius: 17px; font-weight: 700; font-size: 12px; }
QWidget#QuickLinks { background: white; border-bottom: 1px solid #E6EDF7; }
QPushButton#QuickLink {
  background: transparent; border: none; border-radius: 8px; padding: 6px 12px; font-size: 12px; font-weight: 600; color: #3D4F78;
}
QPushButton#QuickLink:hover { background: #EEF4FF; color: #2864BA; }
QLabel#TopReady { color: #8A97B4; font-size: 11px; }

/* ===== Bảng lệnh nhanh ===== */
QDialog#QuickPalette { background: white; border: 1px solid #C9D7EE; border-radius: 14px; }
QLineEdit#PaletteInput { border: 1px solid #C9D7EE; border-radius: 10px; padding: 9px 12px; font-size: 14px; }
QListWidget#PaletteList { border: none; background: transparent; font-size: 13px; }
QListWidget#PaletteList::item { padding: 9px 10px; border-radius: 8px; }
QListWidget#PaletteList::item:selected { background: #E8F0FF; color: #17294D; }
QLabel#PaletteHint { color: #8A97B4; font-size: 11px; }

/* ===== Ribbon (màu nhấn theo ứng dụng) ===== */
QWidget#Ribbon { background: white; border-bottom: 1px solid #E6EDF7; }
QTabBar#RibbonTabs { background: transparent; }
QTabBar#RibbonTabs::tab {
  background: transparent; border: none; border-bottom: 2px solid transparent; padding: 7px 14px; color: #4A5A7C; font-size: 13px; margin-right: 2px;
}
QTabBar#RibbonTabs::tab:selected { color: @ACC@; border-bottom: 2px solid @ACC@; font-weight: 700; }
QTabBar#RibbonTabs::tab:hover { color: @ACC@; }
QToolButton#RibbonButton {
  background: transparent; border: 1px solid transparent; border-radius: 8px; padding: 3px 4px; font-size: 11px; min-width: 54px; color: #3D4F78;
}
QToolButton#RibbonButton:hover { background: @ACCBG@; border-color: @ACCBD@; }
QToolButton#RibbonButton:checked { background: @ACCBG2@; border-color: @ACC@; }
QToolButton#RibbonButton:disabled { color: #A9B4CA; }
QLabel#GroupLabel { color: #8A97B4; font-size: 10px; padding-top: 2px; }
QFrame#GroupSep { color: #E6EDF7; }
)");
    // Màu nhấn mặc định (xanh Word); Ribbon ghi đè theo thuộc tính kind bằng các khối bên dưới.
    css.replace(QStringLiteral("@ACCBG2@"), QStringLiteral("#DCE9FF"));
    css.replace(QStringLiteral("@ACCBG@"), QStringLiteral("#EEF4FF"));
    css.replace(QStringLiteral("@ACCBD@"), QStringLiteral("#C9DAF5"));
    css.replace(QStringLiteral("@ACC@"), QStringLiteral("#2864BA"));
    struct K { const char* kind; const char* acc; const char* bg; const char* bg2; const char* bd; };
    const K ks[] = {{"excel", "#16804E", "#EAF7F0", "#D5EFE2", "#BFE3D0"},
                    {"ppt", "#C45530", "#FDF0EA", "#FADFD3", "#F2C9B8"},
                    {"pdf", "#B94449", "#FCEEEF", "#F8DCDE", "#EFC3C6"}};
    for (const K& k : ks) {
        const QString pre = QStringLiteral("QWidget#Ribbon[kind=\"%1\"] ").arg(QLatin1String(k.kind));
        css += pre + QStringLiteral("QTabBar#RibbonTabs::tab:selected { color: %1; border-bottom: 2px solid %1; }\n").arg(QLatin1String(k.acc));
        css += pre + QStringLiteral("QTabBar#RibbonTabs::tab:hover { color: %1; }\n").arg(QLatin1String(k.acc));
        css += pre + QStringLiteral("QToolButton#RibbonButton:hover { background: %1; border-color: %2; }\n").arg(QLatin1String(k.bg), QLatin1String(k.bd));
        css += pre + QStringLiteral("QToolButton#RibbonButton:checked { background: %1; border-color: %2; }\n").arg(QLatin1String(k.bg2), QLatin1String(k.acc));
    }
    css += QStringLiteral(R"(
/* ===== Thẻ tài liệu ===== */
QTabWidget#DocTabs::pane { border: none; }
QTabBar#DocTabBar { background: #F4F8FF; }
QTabBar#DocTabBar::tab {
  background: #EAF0FA; border: 1px solid #DCE5F2; border-bottom: none;
  padding: 6px 12px; margin-right: 3px; border-top-left-radius: 8px; border-top-right-radius: 8px;
  min-width: 120px; max-width: 240px;
}
QTabBar#DocTabBar::tab:selected { background: white; font-weight: 700; }

/* ===== Thanh công thức / ô nhập ===== */
QWidget#FormulaBar { background: white; border-bottom: 1px solid #E6EDF7; }
QLineEdit { border: 1px solid #DCE5F2; border-radius: 6px; padding: 4px 8px; background: white; selection-background-color: #BFD6F7; selection-color: #17294D; }
QLineEdit:focus { border-color: #2864BA; }
QLabel#Fx { color: #6B7A99; font-style: italic; font-weight: 600; }

QFrame#Banner { background: #FFF7E0; border-bottom: 1px solid #F5D98B; }
QLabel#BannerText { color: #7A5B00; }

QTabBar#SheetTabs::tab { background: #EEF2F7; border: 1px solid #DCE5F2; border-top: none; padding: 4px 14px; margin-right: 1px; }
QTabBar#SheetTabs::tab:selected { background: white; color: #16804E; font-weight: 700; border-bottom: 2px solid #16804E; }

/* ===== Trang chủ ===== */
QWidget#Home, QScrollArea#HomeScroll, QWidget#HomeBody { background: #F4F8FF; border: none; }
QFrame#HeroBanner {
  border: 1px solid #D5E3FA; border-radius: 22px;
  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #F6FAFF, stop:0.6 #E9F1FF, stop:1 #E3EDFF);
}
QLabel#HeroKicker { color: #2864BA; font-size: 11px; font-weight: 700; letter-spacing: 2px; background: transparent; }
QLabel#HeroTitle { font-size: 30px; font-weight: 700; color: #17294D; background: transparent; }
QLabel#HeroText { color: #5B6B8C; font-size: 13px; background: transparent; }
QPushButton#HeroButton {
  background: white; border: 1px solid #DCE5F2; border-radius: 12px; padding: 11px 20px; font-weight: 700; color: #1B4A90;
}
QPushButton#HeroButton:hover { background: #F2F7FF; border-color: #2864BA; }
QLabel#SectionTitle { font-size: 17px; font-weight: 700; color: #17294D; }
QLabel#SectionNote { font-size: 11px; color: #8A97B4; }
QFrame#FeatureCard, QFrame#QuickCard {
  background: white; border: 1px solid #DCE5F2; border-radius: 18px;
}
QFrame#FeatureCard:hover, QFrame#QuickCard:hover { border-color: #2864BA; background: #FAFCFF; }
QFrame#FeatureCard QLabel, QFrame#QuickCard QLabel { background: transparent; border: none; }
QLabel#CardTitle { font-size: 19px; font-weight: 700; color: #17294D; }
QLabel#CardDesc { font-size: 12px; color: #6B7A99; }
QLabel#CardLink { font-size: 11px; font-weight: 700; color: #2864BA; }
QLabel#QuickTitle { font-size: 13px; font-weight: 700; color: #17294D; }
QLabel#QuickDesc { font-size: 11px; color: #6B7A99; }
QFrame#InfoBanner { background: #EAF7F0; border: 1px solid #BFE3D0; border-radius: 16px; }
QFrame#InfoBanner QLabel { background: transparent; }
QLabel#InfoKicker { color: #16804E; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
QLabel#InfoText { color: #2F5D49; font-size: 12px; }
QLabel#HomeSubtitle { color: #6B7A99; font-size: 12px; }
QPushButton#Primary { background: #2864BA; color: white; border: none; border-radius: 10px; padding: 8px 18px; font-weight: 700; }
QPushButton#Primary:hover { background: #1B4A90; }
QListWidget#Recent { background: white; border: 1px solid #DCE5F2; border-radius: 14px; padding: 4px; }
QListWidget#Recent::item { padding: 9px; border-radius: 8px; }
QListWidget#Recent::item:hover { background: #EEF4FF; }

/* ===== Thanh trạng thái ===== */
QStatusBar { background: white; border-top: 1px solid #E6EDF7; color: #4A5A7C; min-height: 32px; }
QStatusBar::item { border: none; }
QStatusBar QLabel { padding: 0 8px; color: #4A5A7C; font-size: 11px; }
)");
    return css;
}

} // namespace theme
