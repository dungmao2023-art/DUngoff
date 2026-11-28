// SPDX-License-Identifier: MPL-2.0
#include "SidePanel.h"

#include "WorkspaceStore.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
#include <QTextCharFormat>
#include <QVector>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QSplitter>
#include <QLocale>
#include <QTabWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDateEdit>
#include <QCheckBox>
#include <QDrag>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QUuid>
#include <functional>
#include <QHash>
#include <QSet>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QStyle>

namespace {
const QStringList kWorkStatus{QObject::tr("Chưa làm"), QObject::tr("Đang làm"), QObject::tr("Chờ duyệt"), QObject::tr("Hoàn thành")};
const QStringList kPriority{QObject::tr("Thấp"), QObject::tr("Vừa"), QObject::tr("Cao")};
QString uid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString field(const QJsonObject& o, const char* k) { return o.value(QLatin1String(k)).toString(); }
QString personName(const QJsonObject& p, const QString& id) {
    for(const auto& v:p.value(QStringLiteral("people")).toArray()) {
        const auto person=v.toObject();
        if(field(person,"id")==id) return field(person,"name");
    }
    return {};
}
enum CardRole { CardTitle=Qt::UserRole+10, CardGroup, CardPriority, CardDue, CardPerson, CardOverdue };
QString initials(const QString& name) {
    const auto words=name.simplified().split(QLatin1Char(' '),Qt::SkipEmptyParts);
    if(words.isEmpty())return QStringLiteral("—");
    return (words.size()==1?words.first().left(2):words.first().left(1)+words.last().left(1)).toUpper();
}
class CardDelegate : public QStyledItemDelegate {
public:
    explicit CardDelegate(QObject* parent):QStyledItemDelegate(parent){}
    QSize sizeHint(const QStyleOptionViewItem&,const QModelIndex&) const override {return QSize(205,146);}
    void paint(QPainter* p,const QStyleOptionViewItem& opt,const QModelIndex& idx) const override {
        p->save();p->setRenderHint(QPainter::Antialiasing);
        const QRectF r=QRectF(opt.rect).adjusted(3,2,-3,-4);
        const bool selected=opt.state.testFlag(QStyle::State_Selected);
        p->setPen(Qt::NoPen);p->setBrush(QColor(35,56,76,13));p->drawRoundedRect(r.translated(0,2),8,8);
        p->setBrush(QColor("#FFFFFF"));p->setPen(QPen(QColor(selected?"#7296DE":"#DCE3EB"),selected?1.6:1.0));p->drawRoundedRect(r,8,8);
        const qreal x=r.left()+12, w=qMax(10.0,r.width()-24), y=r.top();
        const auto group=idx.data(CardGroup).toString(),who=idx.data(CardPerson).toString();
        const QStringList dots{QStringLiteral("#7394D9"),QStringLiteral("#50A494"),QStringLiteral("#E2A35C"),QStringLiteral("#A48BD3")};
        const auto dot=dots.at(qHash(group)%dots.size());
        QFont small=opt.font;small.setPixelSize(11);small.setWeight(QFont::Normal);
        QFont bold=opt.font;bold.setPixelSize(13);bold.setWeight(QFont::DemiBold);
        p->setPen(Qt::NoPen);p->setBrush(QColor(dot));p->drawEllipse(QRectF(x,y+15,9,9));
        p->setFont(small);p->setPen(QColor("#677B96"));
        p->drawText(QRectF(x+15,y+9,w-15,22),Qt::AlignVCenter,QFontMetrics(small).elidedText(group.isEmpty()?QObject::tr("Chưa phân nhóm"):group,Qt::ElideRight,qMax(0,int(w-15))));
        p->setFont(bold);p->setPen(QColor("#16293E"));
        const auto title=idx.data(CardTitle).toString();
        const QRectF titleRect(x,y+32,w,37);
        p->save();p->setClipRect(titleRect);p->drawText(titleRect,Qt::AlignTop|Qt::TextWordWrap,title);p->restore();
        const int pri=qBound(0,idx.data(CardPriority).toInt(),2);
        const QStringList bg{QStringLiteral("#EDF1F5"),QStringLiteral("#FFF2DA"),QStringLiteral("#FCE9E7")};
        const QStringList fg{QStringLiteral("#62788E"),QStringLiteral("#A26B17"),QStringLiteral("#B8423B")};
        p->setFont(small);const auto badge=kPriority.at(pri);const int bw=QFontMetrics(small).horizontalAdvance(badge)+16;
        p->setPen(Qt::NoPen);p->setBrush(QColor(bg.at(pri)));p->drawRoundedRect(QRectF(x,y+73,bw,23),5,5);
        p->setPen(QColor(fg.at(pri)));p->drawText(QRectF(x,y+73,bw,23),Qt::AlignCenter,badge);
        p->setPen(QColor(idx.data(CardOverdue).toBool()?"#B8423B":"#687C98"));
        p->drawText(QRectF(x,y+108,qMax(0.0,w-38),24),Qt::AlignVCenter,QFontMetrics(small).elidedText(idx.data(CardDue).toString(),Qt::ElideRight,qMax(0,int(w-38))));
        const QRectF avatar(r.right()-42,y+104,30,30);p->setPen(Qt::NoPen);p->setBrush(QColor("#E4ECE8"));p->drawEllipse(avatar);
        small.setWeight(QFont::DemiBold);p->setFont(small);p->setPen(QColor("#285D60"));p->drawText(avatar,Qt::AlignCenter,initials(who));
        p->restore();
    }
};
const char* kTaskMime="application/x-dungoffice-work-task";
class BoardList : public QListWidget {
public:
    explicit BoardList(QWidget* parent):QListWidget(parent) {
        setDragEnabled(true);setAcceptDrops(true);viewport()->setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::MoveAction);setDropIndicatorShown(false);
        setWordWrap(true);setSpacing(4);setMinimumWidth(218);setItemDelegate(new CardDelegate(this));setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
    std::function<void(const QString&)> dropped;
protected:
    void startDrag(Qt::DropActions) override {
        auto* item=currentItem();if(!item)return;
        auto* mime=new QMimeData;mime->setData(kTaskMime,item->data(Qt::UserRole).toString().toUtf8());
        auto* drag=new QDrag(this);drag->setMimeData(mime);drag->exec(Qt::MoveAction);drag->deleteLater();
    }
    void dragEnterEvent(QDragEnterEvent* e) override {
        if(qobject_cast<QListWidget*>(e->source()) && e->mimeData()->hasFormat(kTaskMime)) e->acceptProposedAction();else e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent* e) override {
        if(qobject_cast<QListWidget*>(e->source()) && e->mimeData()->hasFormat(kTaskMime)) e->acceptProposedAction();else e->ignore();
    }
    void dropEvent(QDropEvent* e) override {
        if(!qobject_cast<QListWidget*>(e->source())||!e->mimeData()->hasFormat(kTaskMime)){e->ignore();return;}
        const QString id=QString::fromUtf8(e->mimeData()->data(kTaskMime));
        e->acceptProposedAction();
        if(dropped)dropped(id);
    }
};
const QStringList kStatuses = {QObject::tr("Mới"), QObject::tr("Đang liên hệ"), QObject::tr("Chờ phản hồi"),
                               QObject::tr("Hoàn tất"), QObject::tr("Ngừng")};
}

SidePanel::SidePanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("SidePanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("WorkContent"));
    m_content->setAttribute(Qt::WA_StyledBackground, true);
    auto* v=new QVBoxLayout(m_content);
    v->setContentsMargins(22,18,22,16);v->setSpacing(14);
    m_title=new QLabel(tr("Ghi chú"),m_content);m_title->setObjectName(QStringLiteral("WorkTitle"));
    v->addWidget(m_title);
    m_tabs=new QStackedWidget(m_content);
    for(QWidget* page:{buildNotes(),buildWorkManager(),buildContacts()}){
        auto* scroll=new QScrollArea(m_tabs);scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);scroll->setWidget(page);m_tabs->addWidget(scroll);
    }
    v->addWidget(m_tabs,1);
    auto* foot=new QLabel(tr("Dữ liệu lưu cục bộ · Không tự gửi lên mạng"),m_content);
    foot->setObjectName(QStringLiteral("WorkHint"));foot->setWordWrap(true);v->addWidget(foot);
    auto* saveRow=new QHBoxLayout;
    m_saveStatus=new QLabel(tr("Đã lưu"),m_content);
    m_saveStatus->setObjectName(QStringLiteral("WorkspaceSaveStatus"));
    m_saveStatus->setWordWrap(true);
    m_retrySave=new QPushButton(tr("Lưu ngay"),m_content);
    m_retrySave->setObjectName(QStringLiteral("WorkspaceSaveNow"));
    m_retrySave->setEnabled(false);
    saveRow->addWidget(m_saveStatus,1);
    saveRow->addWidget(m_retrySave);
    v->addLayout(saveRow);
    connect(m_retrySave,&QPushButton::clicked,this,[this]{ flush(); });
    outer->addWidget(m_content,1);
    setStyleSheet(QStringLiteral(R"(
QWidget#SidePanel, QWidget#WorkRail { background: #FBFCFF; }
QWidget#WorkRail { border-left: 1px solid #DCE5F2; }
QWidget#WorkContent { background: #F4F8FF; border-left: 1px solid #DCE5F2; }
QLabel#WorkTitle { font-size: 25px; font-weight: 700; color: #17294D; }
QLabel#WorkHint { font-size: 11px; color: #6B7A99; }
QPushButton#WorkTab { background: transparent; border: none; border-right: 3px solid transparent; border-radius: 8px; font-size: 11px; color: #4A5A7C; }
QPushButton#WorkTab:hover, QPushButton#WorkTab:focus { background: #EEF4FF; }
QPushButton#WorkTab:checked { background: #E8F0FF; border-right: 3px solid #2864BA; color: #1B4A90; font-weight: 700; }
QWidget#WorkContent QPushButton { background: white; border: 1px solid #DCE5F2; border-radius: 8px; padding: 5px 8px; color: #1B4A90; }
QWidget#WorkContent QPushButton:hover, QWidget#WorkContent QPushButton:focus { background: #EEF4FF; border-color: #2864BA; }
QWidget#WorkContent QPushButton#WorkPrimary { background: #2864BA; color: white; font-weight: 600; }
QWidget#WorkContent QPushButton#WorkPrimary:hover { background: #1B4A90; }
QWidget#WorkContent QPushButton#WorkClose { padding: 0; font-size: 18px; }
QWidget#WorkContent QPlainTextEdit, QWidget#WorkContent QListWidget, QWidget#WorkContent QTableWidget { background: white; border: 1px solid #DCE5F2; border-radius: 8px; padding: 4px; selection-background-color: #E8F0FF; selection-color: #17294D; }
QWidget#WorkContent QPlainTextEdit:focus, QWidget#WorkContent QLineEdit:focus { border-color: #2864BA; }
QWidget#WorkContent QListWidget::item { padding: 6px; border-radius: 4px; }
QWidget#WorkContent QHeaderView::section { background: #EEF4FF; border: none; padding: 6px; font-weight: 600; color: #4A5A7C; }
QWidget#WorkContent QComboBox { background: white; border: 1px solid #DCE5F2; border-radius: 6px; padding: 4px; }
QWidget#WorkContent QCalendarWidget { background: white; border: 1px solid #DCE5F2; border-radius: 8px; }
QWidget#WorkContent QCalendarWidget QToolButton { color: #1B4A90; background: #EEF4FF; border: none; padding: 3px; }
QWidget#WorkContent QCalendarWidget QWidget#qt_calendar_navigationbar { background: #EEF4FF; }
QWidget#WorkContent QCalendarWidget QAbstractItemView { selection-background-color: #2864BA; selection-color: white; }
)"));
    showPage(0);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(800);
    connect(m_saveTimer, &QTimer::timeout, this, &SidePanel::saveNow);
    load();
    if(!m_readable) {
        m_tabs->setEnabled(false);
        m_saveStatus->setText(tr("Chỉ đọc"));
        foot->setText(tr("Không thể đọc dữ liệu công việc an toàn. Tạm khóa chỉnh sửa để tránh ghi đè; hãy kiểm tra tệp dữ liệu và mở lại ứng dụng."));
        foot->setStyleSheet(QStringLiteral("color:#A33626;font-size:12px;"));
    }
}

void SidePanel::showPage(int index) {
    if(index<0||index>=m_tabs->count())return;
    static const QStringList titles{tr("Ghi chú"),tr("Quản lý công việc"),tr("Danh bạ liên hệ")};
    m_tabs->setCurrentIndex(index);
    m_title->setText(titles[index]);
    m_lastPage=index;
}

void SidePanel::toggleContent() { showPage(m_lastPage); }

bool SidePanel::flush() {
    m_saveTimer->stop();
    return !m_dirty || saveNow();
}

void SidePanel::scheduleSave() {
    if (!m_loading && m_readable) {
        m_dirty=true;
        m_saveStatus->setText(tr("Chưa lưu"));
        m_retrySave->setEnabled(true);
        m_saveTimer->start();
    }
}

bool SidePanel::saveNow() {
    if(!m_dirty) return true;
    if(!m_readable) return false;
    QJsonObject o=m_workspaceBase;
    o.insert(QStringLiteral("notes"), m_notes);
    o.insert(QStringLiteral("events"), m_events);
    o.insert(QStringLiteral("dayNotes"), m_dayNotes);
    o.insert(QStringLiteral("contacts"), m_contacts);
    o.insert(QStringLiteral("workProjects"), m_workProjects);
    o.insert(QStringLiteral("workTasks"), m_tasks);
    o.insert(QStringLiteral("version"), 1);
    if (!workspace::save(QStringLiteral("workspace"), o)) {
        m_saveStatus->setText(tr("Lưu thất bại — nội dung vẫn còn. Kiểm tra thư mục dữ liệu và bấm Lưu ngay."));
        m_retrySave->setEnabled(true);
        return false;
    }
    m_workspaceBase=o;
    m_dirty=false;
    m_saveStatus->setText(tr("Đã lưu"));
    m_retrySave->setEnabled(false);
    return true;
}

void SidePanel::load() {
    m_loading = true;
    const QJsonObject o = workspace::load(QStringLiteral("workspace"),&m_readable);
    if(o.value(QStringLiteral("version")).toInt(1)>1) m_readable=false;
    m_workspaceBase=o;
    m_notes = o.value(QStringLiteral("notes")).toArray();
    m_events = o.value(QStringLiteral("events")).toArray();
    m_dayNotes = o.value(QStringLiteral("dayNotes")).toObject();
    m_contacts = o.value(QStringLiteral("contacts")).toArray();
    m_workProjects = o.value(QStringLiteral("workProjects")).toArray();
    m_tasks = o.value(QStringLiteral("workTasks")).toArray();
    refreshProjects();
    refreshNoteList();
    refreshEvents();
    refreshContacts();
    markCalendar();
    m_loading = false;
}

// ---------------------------------------------------------------- Ghi chú
QWidget* SidePanel::buildNotes() {
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(tr("+ Ghi chú mới"), w);
    add->setObjectName(QStringLiteral("WorkPrimary"));
    auto* del = new QPushButton(tr("Xoá"), w);
    row->addWidget(add);
    row->addWidget(del);
    v->addLayout(row);
    m_noteList = new QListWidget(w);
    auto* split=new QSplitter(Qt::Horizontal,w);
    m_noteList->setMinimumWidth(160);
    split->addWidget(m_noteList);
    m_noteText = new QPlainTextEdit(w);
    m_noteText->setObjectName(QStringLiteral("WorkspaceNoteEditor"));
    m_noteText->setPlaceholderText(tr("Gõ ghi chú ở đây. Dùng \"[ ] việc cần làm\" và \"[x] việc đã xong\" để lập danh sách việc."));
    split->addWidget(m_noteText);split->setStretchFactor(1,1);split->setSizes({220,700});
    v->addWidget(split,1);

    connect(add, &QPushButton::clicked, this, [this] {
        QJsonObject n;
        n.insert(QStringLiteral("id"), QString::number(QDateTime::currentMSecsSinceEpoch()));
        n.insert(QStringLiteral("text"), QString());
        n.insert(QStringLiteral("updated"), QDateTime::currentDateTime().toString(Qt::ISODate));
        m_notes.prepend(n);
        refreshNoteList();
        m_noteList->setCurrentRow(0);
        m_noteText->setFocus();
        scheduleSave();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const int r = m_noteList->currentRow();
        if (r < 0 || r >= m_notes.size()) return;
        if (QMessageBox::question(this, tr("Xoá ghi chú"), tr("Xoá ghi chú đang chọn?")) != QMessageBox::Yes) return;
        m_notes.removeAt(r);
        m_noteRow = -1;
        refreshNoteList();
        scheduleSave();
    });
    connect(m_noteList, &QListWidget::currentRowChanged, this, [this](int r) {
        m_noteRow = r;
        QSignalBlocker b(m_noteText);
        m_noteText->setPlainText(r >= 0 && r < m_notes.size() ? m_notes[r].toObject().value(QStringLiteral("text")).toString() : QString());
        m_noteText->setEnabled(r >= 0);
    });
    connect(m_noteText, &QPlainTextEdit::textChanged, this, [this] {
        if (m_noteRow < 0 || m_noteRow >= m_notes.size()) return;
        QJsonObject n = m_notes[m_noteRow].toObject();
        n.insert(QStringLiteral("text"), m_noteText->toPlainText());
        n.insert(QStringLiteral("updated"), QDateTime::currentDateTime().toString(Qt::ISODate));
        m_notes[m_noteRow] = n;
        if (auto* it = m_noteList->item(m_noteRow)) {
            const QString first = m_noteText->toPlainText().section(QLatin1Char('\n'), 0, 0).trimmed();
            it->setText(first.isEmpty() ? tr("(ghi chú trống)") : first.left(40));
        }
        scheduleSave();
    });
    return w;
}

void SidePanel::refreshNoteList() {
    QSignalBlocker b(m_noteList);
    m_noteList->clear();
    for (const QJsonValue& v : m_notes) {
        const QString first = v.toObject().value(QStringLiteral("text")).toString().section(QLatin1Char('\n'), 0, 0).trimmed();
        m_noteList->addItem(first.isEmpty() ? tr("(ghi chú trống)") : first.left(40));
    }
    m_noteRow = -1;
    m_noteText->setEnabled(false);
    {
        QSignalBlocker bt(m_noteText);
        m_noteText->clear();
    }
    if (m_noteList->count() > 0) {
        m_noteList->setCurrentRow(0);
        m_noteRow = 0;
        m_noteText->setEnabled(true);
        QSignalBlocker bt(m_noteText);
        m_noteText->setPlainText(m_notes[0].toObject().value(QStringLiteral("text")).toString());
    }
}

// ---------------------------------------------------------------- Quản lý công việc
int SidePanel::projectIndex() const {
    const auto id=m_projectPicker->currentData().toString();
    for(int i=0;i<m_workProjects.size();++i) if(field(m_workProjects[i].toObject(),"id")==id)return i;
    return -1;
}
int SidePanel::taskIndex(const QString& id) const {
    const int pi=projectIndex();if(pi<0||id.isEmpty())return -1;
    const auto project=field(m_workProjects[pi].toObject(),"id");
    for(int i=0;i<m_tasks.size();++i) {
        const auto task=m_tasks[i].toObject();
        if(field(task,"id")==id&&field(task,"project")==project)return i;
    }
    return -1;
}
QWidget* SidePanel::buildWorkManager() {
    auto* w=new QWidget(this);auto* v=new QVBoxLayout(w);
    auto* row=new QHBoxLayout;
    row->addWidget(new QLabel(tr("Dự án"),w));
    m_projectPicker=new QComboBox(w);m_projectPicker->setObjectName(QStringLiteral("WorkProjectPicker"));
    row->addWidget(m_projectPicker,1);
    auto* add=new QPushButton(tr("+ Dự án"),w);add->setObjectName(QStringLiteral("WorkAddProject"));row->addWidget(add);
    m_editProject=new QPushButton(tr("Sửa dự án / Thành viên"),w);m_editProject->setObjectName(QStringLiteral("WorkEditProject"));row->addWidget(m_editProject);
    v->addLayout(row);
    m_workViews=new QTabWidget(w);m_workViews->setObjectName(QStringLiteral("WorkViews"));
    m_workViews->setStyleSheet(QStringLiteral("QTabBar::tab {padding:9px 24px;background:#EDF1F4;color:#58697F;border-radius:6px;margin-right:4px;} QTabBar::tab:selected {background:white;color:#267B69;font-weight:600;} QTabWidget::pane {border:0;}"));
    auto* boardPage=new QWidget(w);auto* bv=new QVBoxLayout(boardPage);
    auto* filters=new QHBoxLayout;m_taskSearch=new QLineEdit(boardPage);m_taskSearch->setPlaceholderText(tr("Tìm công việc, nhóm, nội dung…"));m_taskSearch->setClearButtonEnabled(true);m_taskSearch->setObjectName(QStringLiteral("WorkTaskSearch"));
    m_taskPersonFilter=new QComboBox(boardPage);m_taskPersonFilter->setObjectName(QStringLiteral("WorkPersonFilter"));filters->addWidget(m_taskSearch,1);filters->addWidget(m_taskPersonFilter);bv->addLayout(filters);
    connect(m_taskSearch,&QLineEdit::textChanged,this,[this]{refreshBoard();});connect(m_taskPersonFilter,&QComboBox::currentIndexChanged,this,[this]{refreshBoard();});
    auto* hint=new QLabel(tr("Kéo thẻ sang cột khác để đổi trạng thái. Nhấp đúp để sửa, chọn người phụ trách và hạn chót."),boardPage);
    hint->setWordWrap(true);bv->addWidget(hint);
    auto* sc=new QScrollArea(boardPage);sc->setWidgetResizable(true);sc->setFrameShape(QFrame::NoFrame);
    auto* columns=new QWidget;auto* ch=new QHBoxLayout(columns);ch->setContentsMargins(0,0,0,0);
    const QStringList colors{QStringLiteral("#58697F"),QStringLiteral("#365CB5"),QStringLiteral("#A36B13"),QStringLiteral("#20795F")};
    for(int i=0;i<4;++i){
        auto* col=new QFrame(columns);col->setObjectName(QStringLiteral("WorkLane"));col->setStyleSheet(QStringLiteral("QFrame#WorkLane { background:#EDF1F4; border-radius:9px; }"));
        auto* cv=new QVBoxLayout(col);cv->setContentsMargins(6,8,6,6);auto* head=new QHBoxLayout;head->setContentsMargins(5,0,3,0);
        m_columnTitles[i]=new QLabel(kWorkStatus[i],col);
        m_columnTitles[i]->setStyleSheet(QStringLiteral("font-weight:600;color:%1;padding:5px;").arg(colors[i]));
        head->addWidget(m_columnTitles[i],1);
        m_addTask[i]=new QPushButton(QStringLiteral("+"),col);m_addTask[i]->setObjectName(QStringLiteral("WorkAddTask%1").arg(i));
        m_addTask[i]->setAccessibleName(tr("Thêm việc: %1").arg(kWorkStatus[i]));m_addTask[i]->setFixedSize(30,30);
        head->addWidget(m_addTask[i]);cv->addLayout(head);
        auto* list=new BoardList(col);list->setObjectName(QStringLiteral("WorkColumn%1").arg(i));
        list->setStyleSheet(QStringLiteral("QListWidget {background:#EDF1F4;border:0;padding:0;} QListWidget::item {border:0;padding:0;}"));
        m_board[i]=list;cv->addWidget(list,1);ch->addWidget(col,1);
        list->dropped=[this,i](const QString& id){moveTask(id,i);};
        connect(list,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* it){const auto id=it->data(Qt::UserRole).toString();editTask(id);});
        connect(m_addTask[i],&QPushButton::clicked,this,[this,i]{editTask({},i);});
    }
    sc->setWidget(columns);bv->addWidget(sc,1);
    m_workViews->addTab(boardPage,tr("Kanban"));
    m_workViews->addTab(buildCalendar(),tr("Lịch"));
    v->addWidget(m_workViews,1);
    connect(add,&QPushButton::clicked,this,[this]{editProject(true);});
    connect(m_editProject,&QPushButton::clicked,this,[this]{editProject(false);});
    connect(m_projectPicker,&QComboBox::currentIndexChanged,this,[this]{refreshBoard();refreshEvents();markCalendar();});
    return w;
}
void SidePanel::refreshProjects(const QString& select) {
    const QString keep=select.isEmpty()?m_projectPicker->currentData().toString():select;
    {QSignalBlocker b(m_projectPicker);m_projectPicker->clear();
    m_projectPicker->addItem(tr("Chọn dự án"));
    for(const auto& v:m_workProjects){const auto p=v.toObject();m_projectPicker->addItem(field(p,"name"),field(p,"id"));}
    const int idx=m_projectPicker->findData(keep);m_projectPicker->setCurrentIndex(idx>0?idx:(m_projectPicker->count()>1?1:0));}
    refreshBoard();refreshEvents();markCalendar();
}
void SidePanel::refreshBoard() {
    const int pi=projectIndex();m_editProject->setEnabled(pi>=0);
    const auto previous=m_taskPersonFilter->currentData().toString();
    {QSignalBlocker b(m_taskPersonFilter);m_taskPersonFilter->clear();
    m_taskPersonFilter->addItem(tr("Tất cả thành viên"),QString());
    m_taskPersonFilter->addItem(tr("Chưa giao"),QStringLiteral("@unassigned"));
    if(pi>=0)for(const auto& x:m_workProjects[pi].toObject().value(QStringLiteral("people")).toArray()){
        const auto person=x.toObject();m_taskPersonFilter->addItem(field(person,"name"),field(person,"id"));}
    m_taskPersonFilter->setCurrentIndex(qMax(0,m_taskPersonFilter->findData(previous)));}
    const auto personFilter=m_taskPersonFilter->currentData().toString(),search=m_taskSearch->text().trimmed();
    for(int i=0;i<4;++i){m_board[i]->clear();m_addTask[i]->setEnabled(pi>=0);}
    if(pi>=0){
        const auto p=m_workProjects[pi].toObject();
        for(const auto& v:m_tasks){
            const auto task=v.toObject();if(field(task,"project")!=field(p,"id"))continue;
            const int state=qBound(0,task.value(QStringLiteral("status")).toInt(),3);
            const int priority=qBound(0,task.value(QStringLiteral("priority")).toInt(1),2);
            const auto who=personName(p,field(task,"person")),group=field(task,"group");
            if(!personFilter.isEmpty()&&(personFilter==QStringLiteral("@unassigned")?!who.isEmpty():field(task,"person")!=personFilter))continue;
            if(!search.isEmpty()&&!(field(task,"title")+QLatin1Char(' ')+group+QLatin1Char(' ')+field(task,"description")+QLatin1Char(' ')+who).contains(search,Qt::CaseInsensitive))continue;
            const auto date=QDate::fromString(field(task,"due"),Qt::ISODate);
            const QString due=date.isValid()?date.toString(QStringLiteral("dd/MM/yyyy")):tr("Chưa đặt hạn");
            const bool overdue=state!=3&&date.isValid()&&date<QDate::currentDate();
            auto* it=new QListWidgetItem(QStringLiteral("%1\n%2: %3\n%4%5   •   %6")
                .arg(field(task,"title"),tr("Ưu tiên"),kPriority[priority],overdue?tr("Quá hạn: "):QString(),due,who.isEmpty()?tr("Chưa giao"):who));
            it->setData(Qt::UserRole,field(task,"id"));
            it->setData(CardTitle,field(task,"title"));it->setData(CardGroup,group);it->setData(CardPriority,priority);it->setData(CardDue,due);it->setData(CardPerson,who);it->setData(CardOverdue,overdue);
            it->setToolTip((field(task,"title")+QStringLiteral("\n")+group+QStringLiteral("\n")+who+QStringLiteral("\n")+field(task,"description")).toHtmlEscaped().replace(QLatin1Char('\n'),QStringLiteral("<br>")).prepend(QStringLiteral("<qt>")).append(QStringLiteral("</qt>")));
            if(overdue)it->setForeground(QColor("#A33626"));
            m_board[state]->addItem(it);
        }
    }
    for(int i=0;i<4;++i)m_columnTitles[i]->setText(QStringLiteral("%1  %2").arg(kWorkStatus[i]).arg(m_board[i]->count()));
}
void SidePanel::editProject(bool create) {
    const int pi=create?-1:projectIndex();if(!create&&pi<0)return;
    auto p=pi<0?QJsonObject{}:m_workProjects[pi].toObject();
    QDialog d(this);d.setWindowTitle(create?tr("Tạo dự án"):tr("Sửa dự án"));d.resize(640,440);
    auto* v=new QVBoxLayout(&d);auto* form=new QFormLayout;
    auto* name=new QLineEdit(field(p,"name"),&d);name->setObjectName(QStringLiteral("WorkProjectName"));name->setMaxLength(200);form->addRow(tr("Tên dự án"),name);
    v->addLayout(form);v->addWidget(new QLabel(tr("Người liên quan — mỗi người một dòng, liên hệ không bắt buộc"),&d));
    auto* table=new QTableWidget(0,2,&d);table->setObjectName(QStringLiteral("WorkProjectPeople"));table->setHorizontalHeaderLabels({tr("Tên"),tr("Điện thoại / Email / Liên hệ")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);
    auto addPerson=[table](const QJsonObject& person){
        const int row=table->rowCount();table->insertRow(row);
        auto* n=new QTableWidgetItem(field(person,"name"));n->setData(Qt::UserRole,field(person,"id").isEmpty()?uid():field(person,"id"));
        table->setItem(row,0,n);table->setItem(row,1,new QTableWidgetItem(field(person,"contact")));
    };
    for(const auto& x:p.value(QStringLiteral("people")).toArray())addPerson(x.toObject());
    v->addWidget(table,1);auto* row=new QHBoxLayout;
    auto* add=new QPushButton(tr("+ Thêm người"),&d);auto* del=new QPushButton(tr("Bỏ dòng"),&d);
    row->addWidget(add);row->addWidget(del);row->addStretch();v->addLayout(row);
    connect(add,&QPushButton::clicked,&d,[&]{addPerson({});table->editItem(table->item(table->rowCount()-1,0));});
    connect(del,&QPushButton::clicked,&d,[&]{if(table->currentRow()>=0)table->removeRow(table->currentRow());});
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&d);v->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&d,[&]{
        if(name->text().trimmed().isEmpty()){name->setFocus();return;}
        QJsonArray people;
        for(int i=0;i<table->rowCount();++i){
            auto* n=table->item(i,0);const auto contact=table->item(i,1)->text().trimmed();
            if(n->text().trimmed().isEmpty()){if(contact.isEmpty())continue;table->setCurrentCell(i,0);table->editItem(n);return;}
            people.append(QJsonObject{{QStringLiteral("id"),n->data(Qt::UserRole).toString()},{QStringLiteral("name"),n->text().trimmed()},{QStringLiteral("contact"),contact}});
        }
        auto next=p;next.insert(QStringLiteral("people"),people);
        int removed=0;
        for(const auto& x:m_tasks){const auto task=x.toObject();if(field(task,"project")==field(p,"id")&&!field(task,"person").isEmpty()&&personName(next,field(task,"person")).isEmpty())++removed;}
        if(removed&&QMessageBox::question(&d,tr("Cập nhật thành viên"),tr("%1 việc sẽ chuyển về Chưa giao vì người phụ trách bị bỏ khỏi dự án. Tiếp tục?").arg(removed),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        p=next;p.insert(QStringLiteral("name"),name->text().trimmed());if(pi<0)p.insert(QStringLiteral("id"),uid());
        for(int i=0;i<m_tasks.size();++i){auto task=m_tasks[i].toObject();if(field(task,"project")==field(p,"id")&&!field(task,"person").isEmpty()&&personName(p,field(task,"person")).isEmpty()){task.insert(QStringLiteral("person"),QString());m_tasks[i]=task;}}
        if(pi<0)m_workProjects.append(p);else m_workProjects[pi]=p;
        d.accept();
    });
    if(d.exec()==QDialog::Accepted){refreshProjects(field(p,"id"));scheduleSave();}
}
void SidePanel::editTask(const QString& id,int status) {
    const int pi=projectIndex();if(pi<0)return;
    const int ti=taskIndex(id);if(!id.isEmpty()&&ti<0)return;
    const auto p=m_workProjects[pi].toObject();
    auto task=ti<0?QJsonObject{}:m_tasks[ti].toObject();
    QDialog d(this);d.setWindowTitle(ti<0?tr("Thêm công việc"):tr("Sửa công việc"));d.resize(560,510);
    auto* v=new QVBoxLayout(&d);auto* f=new QFormLayout;
    auto* title=new QLineEdit(field(task,"title"),&d);title->setObjectName(QStringLiteral("WorkTaskTitle"));title->setMaxLength(300);f->addRow(tr("Tên công việc"),title);
    auto* group=new QComboBox(&d);group->setObjectName(QStringLiteral("WorkTaskGroup"));group->setEditable(true);group->setInsertPolicy(QComboBox::NoInsert);group->lineEdit()->setMaxLength(100);group->lineEdit()->setPlaceholderText(tr("VD: Kinh doanh, Nội bộ, Sản phẩm"));
    QSet<QString> groups;for(const auto& x:m_tasks){const auto other=x.toObject();if(field(other,"project")==field(p,"id")&&!field(other,"group").isEmpty())groups.insert(field(other,"group"));}
    auto options=groups.values();options.sort(Qt::CaseInsensitive);group->addItems(options);group->setEditText(field(task,"group"));f->addRow(tr("Nhóm công việc"),group);
    auto* desc=new QPlainTextEdit(&d);desc->setPlainText(field(task,"description"));desc->setMaximumHeight(100);f->addRow(tr("Nội dung"),desc);
    auto* st=new QComboBox(&d);st->setObjectName(QStringLiteral("WorkTaskStatus"));st->addItems(kWorkStatus);st->setCurrentIndex(qBound(0,task.value(QStringLiteral("status")).toInt(status),3));f->addRow(tr("Trạng thái"),st);
    auto* pr=new QComboBox(&d);pr->addItems(kPriority);pr->setCurrentIndex(qBound(0,task.value(QStringLiteral("priority")).toInt(1),2));f->addRow(tr("Ưu tiên"),pr);
    auto* who=new QComboBox(&d);who->setObjectName(QStringLiteral("WorkTaskPerson"));who->addItem(tr("Chưa giao"),QString());
    for(const auto& x:p.value(QStringLiteral("people")).toArray()){const auto person=x.toObject();who->addItem(field(person,"name"),field(person,"id"));who->setItemData(who->count()-1,field(person,"contact"),Qt::ToolTipRole);}
    who->setCurrentIndex(qMax(0,who->findData(field(task,"person"))));f->addRow(tr("Người phụ trách"),who);
    auto* dueRow=new QWidget(&d);auto* dr=new QHBoxLayout(dueRow);dr->setContentsMargins(0,0,0,0);
    auto* hasDue=new QCheckBox(tr("Có hạn chót"),dueRow);hasDue->setObjectName(QStringLiteral("WorkTaskHasDue"));auto* due=new QDateEdit(dueRow);due->setCalendarPopup(true);due->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    const auto date=QDate::fromString(field(task,"due"),Qt::ISODate);hasDue->setChecked(date.isValid());due->setDate(date.isValid()?date:QDate::currentDate());due->setEnabled(hasDue->isChecked());
    connect(hasDue,&QCheckBox::toggled,due,&QDateEdit::setEnabled);dr->addWidget(hasDue);dr->addWidget(due);f->addRow(tr("Hạn chót"),dueRow);
    auto* startRow=new QWidget(&d);auto* sr=new QHBoxLayout(startRow);sr->setContentsMargins(0,0,0,0);
    auto* hasStart=new QCheckBox(tr("Có ngày bắt đầu"),startRow);auto* start=new QDateEdit(startRow);start->setCalendarPopup(true);start->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    const auto startDate=QDate::fromString(field(task,"start"),Qt::ISODate);hasStart->setChecked(startDate.isValid());start->setDate(startDate.isValid()?startDate:QDate::currentDate());start->setEnabled(hasStart->isChecked());connect(hasStart,&QCheckBox::toggled,start,&QDateEdit::setEnabled);sr->addWidget(hasStart);sr->addWidget(start);f->addRow(tr("Bắt đầu"),startRow);
    v->addLayout(f);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&d);v->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&d,[&]{
        if(title->text().trimmed().isEmpty()){title->setFocus();return;}
        if(hasStart->isChecked()&&hasDue->isChecked()&&start->date()>due->date()){QMessageBox::warning(&d,tr("Ngày không hợp lệ"),tr("Ngày bắt đầu không thể sau hạn chót."));return;}
        task.insert(QStringLiteral("group"),group->currentText().trimmed());task.insert(QStringLiteral("start"),hasStart->isChecked()?start->date().toString(Qt::ISODate):QString());
        if(ti<0)task.insert(QStringLiteral("id"),uid());
        task.insert(QStringLiteral("project"),field(p,"id"));task.insert(QStringLiteral("title"),title->text().trimmed());
        task.insert(QStringLiteral("description"),desc->toPlainText());task.insert(QStringLiteral("status"),st->currentIndex());
        task.insert(QStringLiteral("priority"),pr->currentIndex());task.insert(QStringLiteral("person"),who->currentData().toString());
        task.insert(QStringLiteral("due"),hasDue->isChecked()?due->date().toString(Qt::ISODate):QString());
        if(ti<0)m_tasks.append(task);else m_tasks[ti]=task;
        d.accept();
    });
    if(ti>=0){auto* del=buttons->addButton(tr("Xóa việc"),QDialogButtonBox::DestructiveRole);
        connect(del,&QPushButton::clicked,&d,[&]{if(QMessageBox::question(&d,tr("Xóa việc"),tr("Xóa công việc này?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes){m_tasks.removeAt(ti);d.accept();}});}
    if(d.exec()==QDialog::Accepted){refreshBoard();refreshEvents();markCalendar();scheduleSave();}
}
QJsonObject SidePanel::exportWorkProject() const {
    const int pi=projectIndex();if(!m_readable||pi<0)return {};
    const auto project=m_workProjects[pi].toObject();QJsonArray tasks;
    for(const auto& x:m_tasks)if(field(x.toObject(),"project")==field(project,"id"))tasks.append(x);
    return QJsonObject{{QStringLiteral("format"),QStringLiteral("dungoffice-kanban")},{QStringLiteral("version"),1},
                       {QStringLiteral("project"),project},{QStringLiteral("tasks"),tasks}};
}
bool SidePanel::importWorkProject(const QJsonObject& snapshot) {
    if(!m_readable||field(snapshot,"format")!=QStringLiteral("dungoffice-kanban")||
       snapshot.value(QStringLiteral("version")).toInt()!=1||!snapshot.value(QStringLiteral("project")).isObject()||
       !snapshot.value(QStringLiteral("tasks")).isArray())return false;
    auto project=snapshot.value(QStringLiteral("project")).toObject();
    const auto oldId=field(project,"id"),newId=uid();
    if(oldId.isEmpty()||field(project,"name").trimmed().isEmpty()||!project.value(QStringLiteral("people")).isArray())return false;
    const auto sourcePeople=project.value(QStringLiteral("people")).toArray();
    const auto sourceTasks=snapshot.value(QStringLiteral("tasks")).toArray();
    if(sourcePeople.size()>10000||sourceTasks.size()>100000)return false;
    QHash<QString,QString> ids;QJsonArray people,tasks;
    for(const auto& x:sourcePeople){
        auto p=x.toObject();const auto old=field(p,"id");
        if(old.isEmpty()||ids.contains(old)||field(p,"name").trimmed().isEmpty())return false;
        const auto next=uid();ids.insert(old,next);p.insert(QStringLiteral("id"),next);people.append(p);
    }
    QSet<QString> taskIds;
    for(const auto& x:sourceTasks){
        auto task=x.toObject();const auto old=field(task,"id"),person=field(task,"person");
        const int state=task.value(QStringLiteral("status")).toInt(-1),priority=task.value(QStringLiteral("priority")).toInt(-1);
        const auto date=field(task,"due"),start=field(task,"start");
        if((!start.isEmpty()&&!QDate::fromString(start,Qt::ISODate).isValid())||(!start.isEmpty()&&!date.isEmpty()&&start>date))return false;
        if(old.isEmpty()||taskIds.contains(old)||field(task,"project")!=oldId||field(task,"title").trimmed().isEmpty()||
           state<0||state>3||priority<0||priority>2||(!person.isEmpty()&&!ids.contains(person))||
           (!date.isEmpty()&&!QDate::fromString(date,Qt::ISODate).isValid()))return false;
        taskIds.insert(old);task.insert(QStringLiteral("id"),uid());task.insert(QStringLiteral("project"),newId);
        task.insert(QStringLiteral("person"),ids.value(person));tasks.append(task);
    }
    project.insert(QStringLiteral("id"),newId);project.insert(QStringLiteral("name"),field(project,"name")+tr(" (bản tải xuống)"));
    project.insert(QStringLiteral("people"),people);
    const auto beforeProjects=m_workProjects,beforeTasks=m_tasks;
    m_workProjects.append(project);for(const auto& task:tasks)m_tasks.append(task);
    scheduleSave();
    if(!flush()){m_workProjects=beforeProjects;m_tasks=beforeTasks;refreshProjects();return false;}
    refreshProjects(newId);return true;
}
void SidePanel::moveTask(const QString& id,int status) {
    const int i=taskIndex(id);if(i<0||status<0||status>3)return;
    auto task=m_tasks[i].toObject();if(task.value(QStringLiteral("status")).toInt()==status)return;
    task.insert(QStringLiteral("status"),status);m_tasks[i]=task;
    refreshBoard();refreshEvents();markCalendar();scheduleSave();
}

// ---------------------------------------------------------------- Lịch
QWidget* SidePanel::buildCalendar() {
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    m_cal = new QCalendarWidget(w);
    m_cal->setGridVisible(true);
    m_cal->setFirstDayOfWeek(Qt::Monday);
    m_cal->setLocale(QLocale(QLocale::Vietnamese,QLocale::Vietnam));
    m_cal->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    auto* split=new QSplitter(Qt::Horizontal,w);
    split->setChildrenCollapsible(false);
    split->addWidget(m_cal);
    auto* details=new QWidget(split);
    auto* detail=new QVBoxLayout(details);
    detail->setContentsMargins(12,0,0,0);
    m_dateTitle=new QLabel(details);
    m_dateTitle->setStyleSheet(QStringLiteral("font-size:18px;font-weight:700;color:#17294D;"));
    detail->addWidget(m_dateTitle);
    auto* today=new QPushButton(tr("Hôm nay"),details);
    connect(today,&QPushButton::clicked,this,[this]{m_cal->setSelectedDate(QDate::currentDate());});
    detail->addWidget(today,0,Qt::AlignLeft);
    detail->addWidget(new QLabel(tr("Ghi chú ngày đang chọn"),details));
    m_dayNote = new QPlainTextEdit(w);
    m_dayNote->setPlaceholderText(tr("Ghi chú riêng cho ngày này…"));
    m_dayNote->setMaximumHeight(100);
    detail->addWidget(m_dayNote);
    connect(m_dayNote, &QPlainTextEdit::textChanged, this, [this] {
        const QString day = m_cal->selectedDate().toString(Qt::ISODate);
        const QString note = m_dayNote->toPlainText();
        if (note.isEmpty()) m_dayNotes.remove(day); else m_dayNotes.insert(day, note);
        markCalendar(); scheduleSave();
    });
    auto* row = new QHBoxLayout;
    m_eventTitle = new QLineEdit(w);
    m_eventTitle->setPlaceholderText(tr("Thêm việc cá nhân cho ngày đang chọn"));
    auto* add = new QPushButton(tr("Thêm"), w);
    add->setObjectName(QStringLiteral("WorkPrimary"));
    row->addWidget(m_eventTitle, 1);
    row->addWidget(add);
    detail->addLayout(row);
    m_eventList = new QListWidget(details);m_eventList->setObjectName(QStringLiteral("WorkCalendarEvents"));
    detail->addWidget(m_eventList,1);
    auto* del = new QPushButton(tr("Xoá mục đang chọn"), w);
    detail->addWidget(del);
    auto* hint = new QLabel(tr("Lịch gồm việc cá nhân và hạn chót của dự án đang chọn. Nhấp đúp để sửa việc dự án hoặc đánh dấu việc cá nhân."), details);
    hint->setStyleSheet(QStringLiteral("color:#697992;font-size:11px;"));
    hint->setWordWrap(true);
    detail->addWidget(hint);
    split->addWidget(details);split->setStretchFactor(0,3);split->setStretchFactor(1,2);
    v->addWidget(split,1);

    auto addEvent = [this] {
        const QString t = m_eventTitle->text().trimmed();
        if (t.isEmpty()) return;
        QJsonObject e;
        e.insert(QStringLiteral("date"), m_cal->selectedDate().toString(Qt::ISODate));
        e.insert(QStringLiteral("title"), t);
        e.insert(QStringLiteral("done"), false);
        m_events.append(e);
        m_eventTitle->clear();
        refreshEvents();
        markCalendar();
        scheduleSave();
    };
    connect(add, &QPushButton::clicked, this, addEvent);
    connect(m_eventTitle, &QLineEdit::returnPressed, this, addEvent);
    connect(m_cal, &QCalendarWidget::selectionChanged, this, &SidePanel::refreshEvents);
    connect(m_eventList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) {
        if(it->data(Qt::UserRole+1).isValid()){const auto id=it->data(Qt::UserRole+1).toString();editTask(id);return;}
        const int idx = it->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_events.size()) return;
        QJsonObject e = m_events[idx].toObject();
        e.insert(QStringLiteral("done"), !e.value(QStringLiteral("done")).toBool());
        m_events[idx] = e;
        refreshEvents();
        scheduleSave();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        auto* it = m_eventList->currentItem();
        if (!it) return;
        if(it->data(Qt::UserRole+1).isValid()){
            const int index=taskIndex(it->data(Qt::UserRole+1).toString());
            if(index>=0&&QMessageBox::question(this,tr("Xóa công việc"),tr("Xóa công việc dự án này khỏi Kanban và Lịch?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes){m_tasks.removeAt(index);refreshBoard();refreshEvents();markCalendar();scheduleSave();}
            return;
        }
        const int idx = it->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_events.size()) return;
        m_events.removeAt(idx);
        refreshEvents();
        markCalendar();
        scheduleSave();
    });
    return w;
}

void SidePanel::refreshEvents() {
    m_eventList->clear();
    const QString day = m_cal->selectedDate().toString(Qt::ISODate);
    m_dateTitle->setText(QLocale(QLocale::Vietnamese,QLocale::Vietnam).toString(m_cal->selectedDate(),QStringLiteral("dddd, dd/MM/yyyy")));
    { QSignalBlocker b(m_dayNote); m_dayNote->setPlainText(m_dayNotes.value(day).toString()); }
    for (int i = 0; i < m_events.size(); ++i) {
        const QJsonObject e = m_events[i].toObject();
        if (e.value(QStringLiteral("date")).toString() != day) continue;
        const bool done = e.value(QStringLiteral("done")).toBool();
        auto* it = new QListWidgetItem((done ? QStringLiteral("☑ ") : QStringLiteral("☐ ")) + e.value(QStringLiteral("title")).toString());
        it->setData(Qt::UserRole, i);
        if (done) {
            QFont f = it->font();
            f.setStrikeOut(true);
            it->setFont(f);
        }
        m_eventList->addItem(it);
    }
    const int pi=projectIndex();if(pi<0)return;
    const auto p=m_workProjects[pi].toObject();
    for(const auto& x:m_tasks){
        const auto task=x.toObject();
        if(field(task,"project")!=field(p,"id")||field(task,"due")!=day)continue;
        const int st=qBound(0,task.value(QStringLiteral("status")).toInt(),3);
        auto* it=new QListWidgetItem(QStringLiteral("[%1] %2 — %3").arg(kWorkStatus[st],field(task,"title"),personName(p,field(task,"person")).isEmpty()?tr("Chưa giao"):personName(p,field(task,"person"))));
        it->setData(Qt::UserRole+1,field(task,"id"));m_eventList->addItem(it);
    }
}

void SidePanel::markCalendar() {
    m_cal->setDateTextFormat(QDate(), QTextCharFormat()); // xoá toàn bộ định dạng ngày cũ
    QTextCharFormat fmt;
    fmt.setFontWeight(QFont::Bold);
    fmt.setBackground(QColor(0xDC, 0xE8, 0xFF));
    for (auto it = m_dayNotes.constBegin(); it != m_dayNotes.constEnd(); ++it) {
        const QDate day = QDate::fromString(it.key(), Qt::ISODate);
        if (day.isValid() && !it.value().toString().isEmpty()) m_cal->setDateTextFormat(day, fmt);
    }
    for (const QJsonValue& v : m_events) {
        const QDate d = QDate::fromString(v.toObject().value(QStringLiteral("date")).toString(), Qt::ISODate);
        if (d.isValid()) m_cal->setDateTextFormat(d, fmt);
    }
    const int pi=projectIndex();if(pi<0)return;
    const auto project=field(m_workProjects[pi].toObject(),"id");
    for(const auto& x:m_tasks){const auto task=x.toObject();if(field(task,"project")!=project)continue;
        const auto date=QDate::fromString(field(task,"due"),Qt::ISODate);if(date.isValid())m_cal->setDateTextFormat(date,fmt);}
}

// ---------------------------------------------------------------- Liên hệ
QWidget* SidePanel::buildContacts() {
    auto* w = new QWidget(this);
    auto* v = new QVBoxLayout(w);
    auto* top = new QHBoxLayout;
    m_search = new QLineEdit(w);
    m_search->setPlaceholderText(tr("Tìm tên, số điện thoại, email…"));
    m_filter = new QComboBox(w);
    m_filter->addItem(tr("Mọi trạng thái"));
    m_filter->addItems(kStatuses);
    top->addWidget(m_search, 1);top->addWidget(m_filter);
    v->addLayout(top);
    m_table = new QTableWidget(0, 4, w);
    m_table->setHorizontalHeaderLabels({tr("Tên"), tr("Điện thoại"), tr("Email"), tr("Trạng thái")});
    m_table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setDefaultSectionSize(36);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->hide();
    v->addWidget(m_table, 1);
    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(tr("+ Liên hệ"), w);
    add->setObjectName(QStringLiteral("WorkPrimary"));
    auto* del = new QPushButton(tr("Xoá"), w);
    row->addWidget(add);
    row->addWidget(del);
    v->addLayout(row);
    auto* sum = new QLabel(w);
    sum->setObjectName(QStringLiteral("ContactSummary"));
    sum->setStyleSheet(QStringLiteral("color:#697992;font-size:11px;"));
    sum->setWordWrap(true);
    v->addWidget(sum);

    connect(add, &QPushButton::clicked, this, [this] {
        QJsonObject c;
        c.insert(QStringLiteral("name"), tr("Liên hệ mới"));
        c.insert(QStringLiteral("phone"), QString());
        c.insert(QStringLiteral("email"), QString());
        c.insert(QStringLiteral("status"), kStatuses.first());
        m_contacts.append(c);
        m_filter->setCurrentIndex(0);
        m_search->clear();
        refreshContacts();
        m_table->selectRow(m_table->rowCount() - 1);
        m_table->editItem(m_table->item(m_table->rowCount() - 1, 0));
        scheduleSave();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const int r = m_table->currentRow();
        if (r < 0) return;
        const int idx = m_table->item(r, 0)->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_contacts.size()) return;
        if (QMessageBox::question(this, tr("Xoá liên hệ"), tr("Xoá liên hệ \"%1\"?").arg(m_table->item(r, 0)->text())) != QMessageBox::Yes) return;
        m_contacts.removeAt(idx);
        refreshContacts();
        scheduleSave();
    });
    connect(m_search, &QLineEdit::textChanged, this, [this] { refreshContacts(); });
    connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { refreshContacts(); });
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* it) {
        if (m_loading || it->column() > 2) return;
        const int idx = m_table->item(it->row(), 0)->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_contacts.size()) return;
        static const char* keys[] = {"name", "phone", "email"};
        QJsonObject c = m_contacts[idx].toObject();
        c.insert(QLatin1String(keys[it->column()]), it->text());
        m_contacts[idx] = c;
        scheduleSave();
    });
    return w;
}

void SidePanel::refreshContacts() {
    const bool wasLoading = m_loading;
    m_loading = true;
    m_table->setRowCount(0);
    const QString q = m_search->text().trimmed().toLower();
    const int f = m_filter->currentIndex() - 1; // -1 = mọi trạng thái
    int shown = 0;
    QVector<int> counts(kStatuses.size(), 0);
    for (int i = 0; i < m_contacts.size(); ++i) {
        const QJsonObject c = m_contacts[i].toObject();
        const int si = qMax(0, kStatuses.indexOf(c.value(QStringLiteral("status")).toString()));
        counts[si]++;
        if (f >= 0 && si != f) continue;
        const QString name = c.value(QStringLiteral("name")).toString(), phone = c.value(QStringLiteral("phone")).toString(),
                      email = c.value(QStringLiteral("email")).toString();
        if (!q.isEmpty() && !(name + QLatin1Char(' ') + phone + QLatin1Char(' ') + email).toLower().contains(q)) continue;
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        auto* n0 = new QTableWidgetItem(name);
        n0->setData(Qt::UserRole, i);
        m_table->setItem(r, 0, n0);
        m_table->setItem(r, 1, new QTableWidgetItem(phone));
        m_table->setItem(r, 2, new QTableWidgetItem(email));
        auto* cb = new QComboBox(m_table);
        cb->addItems(kStatuses);
        cb->setCurrentIndex(si);
        m_table->setCellWidget(r, 3, cb);
        connect(cb, &QComboBox::currentIndexChanged, this, [this, i](int s) {
            if (i < 0 || i >= m_contacts.size()) return;
            QJsonObject c = m_contacts[i].toObject();
            c.insert(QStringLiteral("status"), kStatuses.value(s));
            m_contacts[i] = c;
            scheduleSave();
        });
        ++shown;
    }
    m_table->resizeColumnToContents(1);
    QStringList parts;
    for (int s = 0; s < kStatuses.size(); ++s) parts << QStringLiteral("%1: %2").arg(kStatuses[s]).arg(counts[s]);
    if (auto* sum = findChild<QLabel*>(QStringLiteral("ContactSummary")))
        sum->setText(tr("Hiển thị %1/%2  •  ").arg(shown).arg(m_contacts.size()) + parts.join(QStringLiteral("  •  ")));
    m_loading = wasLoading;
}
