#include "NodeSearchPopup.h"

#include "GraphItems.h"
#include "NodeLibrary.h"

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

namespace sne {

namespace {
constexpr int kTypeRole = Qt::UserRole + 1;
}

NodeSearchPopup::NodeSearchPopup(QWidget* parent) : QWidget(parent, Qt::Popup)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    m_edit = new QLineEdit;
    m_edit->setPlaceholderText("Search nodes…  (↑↓ to pick, Enter to add)");
    m_edit->setClearButtonEnabled(true);
    m_list = new QListWidget;
    m_list->setUniformItemSizes(true);
    m_list->setMinimumSize(320, 300);
    lay->addWidget(m_edit);
    lay->addWidget(m_list);

    m_edit->installEventFilter(this);
    connect(m_edit, &QLineEdit::textChanged, this, &NodeSearchPopup::refill);
    connect(m_list, &QListWidget::itemActivated, this, &NodeSearchPopup::accept);
    connect(m_list, &QListWidget::itemClicked, this, &NodeSearchPopup::accept);
}

void NodeSearchPopup::refill(const QString& filter)
{
    m_list->clear();
    const QStringList words = filter.toLower().split(' ', Qt::SkipEmptyParts);
    for (const NodeDef& d : NodeLibrary::instance().all()) {
        const QString hay = (d.title + " " + d.category + " " + d.description + " " + d.type).toLower();
        bool match = true;
        for (const QString& w : words) if (!hay.contains(w)) { match = false; break; }
        if (!match) continue;
        auto* it = new QListWidgetItem(QString("%1   ›   %2").arg(d.title, d.category));
        it->setData(kTypeRole, d.type);
        it->setToolTip(d.description);
        QPixmap pm(10, 10);
        pm.fill(categoryColor(d.category));
        it->setIcon(QIcon(pm));
        m_list->addItem(it);
    }
    if (m_list->count()) m_list->setCurrentRow(0);
}

void NodeSearchPopup::popup(const QPoint& globalPos)
{
    m_edit->clear();
    refill({});
    adjustSize();
    QPoint p = globalPos;
    if (const QScreen* s = QGuiApplication::screenAt(globalPos)) {
        const QRect r = s->availableGeometry();
        p.setX(std::min(p.x(), r.right() - width()));
        p.setY(std::min(p.y(), r.bottom() - height()));
    }
    move(p);
    show();
    m_edit->setFocus();
}

void NodeSearchPopup::accept(QListWidgetItem* item)
{
    if (!item) return;
    const QString type = item->data(kTypeRole).toString();
    hide();
    emit nodeChosen(type);
}

bool NodeSearchPopup::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == m_edit && e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        switch (k->key()) {
        case Qt::Key_Down:
            m_list->setCurrentRow(std::min(m_list->currentRow() + 1, m_list->count() - 1));
            return true;
        case Qt::Key_Up:
            m_list->setCurrentRow(std::max(m_list->currentRow() - 1, 0));
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            accept(m_list->currentItem());
            return true;
        case Qt::Key_Escape:
            hide();
            return true;
        default: break;
        }
    }
    return QWidget::eventFilter(obj, e);
}

} // namespace sne
