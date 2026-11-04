#include "InspectorWidget.h"

#include "GraphItems.h"
#include "GraphModel.h"

#include <QCheckBox>
#include <QRegularExpression>
#include <algorithm>
#include <memory>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace sne {

namespace {

QLabel* heading(const QString& text)
{
    auto* l = new QLabel(text);
    QFont f = l->font();
    f.setBold(true);
    l->setFont(f);
    l->setStyleSheet("color:#c8c8d2; margin-top:8px;");
    return l;
}

QLabel* note(const QString& text)
{
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setStyleSheet("color:#8a8a96;");
    return l;
}

void styleSwatch(QPushButton* b, const QVariantList& c)
{
    auto ch = [&](int i) { return std::clamp(int((i < c.size() ? c[i].toDouble() : 1.0) * 255.0 + 0.5), 0, 255); };
    b->setText(QString("rgba(%1, %2, %3, %4)")
                   .arg(c.value(0).toDouble(), 0, 'f', 2).arg(c.value(1).toDouble(), 0, 'f', 2)
                   .arg(c.value(2).toDouble(), 0, 'f', 2).arg(c.value(3, 1.0).toDouble(), 0, 'f', 2));
    const QColor col(ch(0), ch(1), ch(2));
    const QString fg = col.lightness() > 140 ? "#111" : "#eee";
    b->setStyleSheet(QString("QPushButton{background:%1; color:%2; border:1px solid #111; padding:4px;}")
                         .arg(col.name(), fg));
}

} // namespace

InspectorWidget::InspectorWidget(GraphModel* model, QWidget* parent) : QWidget(parent), m_model(model)
{
    m_root = new QVBoxLayout(this);
    m_root->setContentsMargins(8, 8, 8, 8);
    m_root->addStretch(1);

    connect(m_model, &GraphModel::graphReset, this, &InspectorWidget::scheduleRebuild);
    connect(m_model, &GraphModel::nodeRemoved, this, &InspectorWidget::scheduleRebuild);
    connect(m_model, &GraphModel::connectionAdded, this, [this](int id) {
        const GraphConnection* c = m_model->connection(id);
        if (c && m_selection.contains(c->toNode)) scheduleRebuild();
    });
    connect(m_model, &GraphModel::connectionRemoved, this, [this](int) { scheduleRebuild(); });
    connect(m_model, &GraphModel::nodeChanged, this, [this](int id) {
        if (!m_internalEdit && m_selection.contains(id)) scheduleRebuild();
    });
    rebuild();
}

void InspectorWidget::setSelection(const QList<int>& ids)
{
    if (ids == m_selection) return;
    m_selection = ids;
    rebuild();
}

void InspectorWidget::scheduleRebuild()
{
    if (m_rebuildPending) return;
    m_rebuildPending = true;
    QTimer::singleShot(0, this, [this] { rebuild(); });
}

void InspectorWidget::rebuild()
{
    m_rebuildPending = false;
    if (m_content) {
        m_root->removeWidget(m_content);
        m_content->deleteLater();
        m_content = nullptr;
    }
    m_content = new QWidget;
    auto* lay = new QVBoxLayout(m_content);
    lay->setContentsMargins(0, 0, 0, 0);
    m_root->insertWidget(0, m_content);

    if (m_selection.size() != 1 || !m_model->node(m_selection.first())) {
        lay->addWidget(note(m_selection.isEmpty()
                                ? "Select a node to edit its properties.\n\n"
                                  "Tab or right-click: add a node\n"
                                  "Drag from a port: connect\n"
                                  "Middle-drag / Space-drag: pan\n"
                                  "Wheel: zoom   F: frame all"
                                : QString("%1 nodes selected.").arg(m_selection.size())));
        return;
    }

    const int id = m_selection.first();
    const NodeDef* d = m_model->def(id);
    if (!d) return;

    auto* title = new QLabel(QString("<b style='font-size:13pt'>%1</b>").arg(d->title));
    lay->addWidget(title);
    lay->addWidget(note(QString("%1  ·  node #%2").arg(d->category).arg(id)));
    lay->addWidget(note(d->description));

    if (!d->params.isEmpty()) {
        lay->addWidget(heading("Parameters"));
        auto* form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        for (int i = 0; i < d->params.size(); ++i) form->addRow(d->params[i].name, makeParamEditor(id, i));
        lay->addLayout(form);
    }

    bool anyDefault = false;
    auto* defForm = new QFormLayout;
    defForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    for (int i = 0; i < d->inputs.size(); ++i) {
        if (m_model->inputConnection(id, i)) continue;
        anyDefault = true;
        defForm->addRow(d->inputs[i].name, makeDefaultEditor(id, i));
    }
    if (anyDefault) {
        lay->addWidget(heading("Unconnected inputs"));
        lay->addLayout(defForm);
    } else {
        delete defForm;
    }
}

QWidget* InspectorWidget::makeParamEditor(int id, int pi)
{
    const ParamDef pd = m_model->def(id)->params[pi];
    const QString name = pd.name;
    const QString mergeKey = QString("param:%1:%2").arg(id).arg(name);
    auto apply = [this, id, name, mergeKey](const QVariant& v) {
        m_internalEdit = true;
        m_model->edit(QString("Change %1").arg(name), [&] { m_model->setParam(id, name, v); }, mergeKey);
        m_internalEdit = false;
    };
    const QVariant cur = m_model->param(id, name);

    switch (pd.kind) {
    case ParamKind::Float: {
        auto* sb = new QDoubleSpinBox;
        sb->setDecimals(4);
        sb->setRange(pd.minValue, pd.maxValue);
        sb->setSingleStep(pd.step);
        sb->setValue(cur.toDouble());
        connect(sb, &QDoubleSpinBox::valueChanged, this, [apply](double v) { apply(v); });
        return sb;
    }
    case ParamKind::Int: {
        auto* sb = new QSpinBox;
        sb->setRange(int(pd.minValue), int(pd.maxValue));
        sb->setValue(cur.toInt());
        connect(sb, &QSpinBox::valueChanged, this, [apply](int v) { apply(v); });
        return sb;
    }
    case ParamKind::Bool: {
        auto* cb = new QCheckBox;
        cb->setChecked(cur.toBool());
        connect(cb, &QCheckBox::toggled, this, [apply](bool v) { apply(v); });
        return cb;
    }
    case ParamKind::Enum: {
        auto* combo = new QComboBox;
        combo->addItems(pd.options);
        combo->setCurrentText(cur.toString());
        connect(combo, &QComboBox::currentTextChanged, this, [apply](const QString& v) { apply(v); });
        return combo;
    }
    case ParamKind::Identifier: {
        auto* le = new QLineEdit(cur.toString());
        le->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z_][A-Za-z0-9_]*"), le));
        connect(le, &QLineEdit::editingFinished, this, [apply, le] { if (!le->text().isEmpty()) apply(le->text()); });
        return le;
    }
    case ParamKind::Code: {
        auto* le = new QLineEdit(cur.toString());
        le->setPlaceholderText("Slang expression");
        QFont mono("monospace");
        mono.setStyleHint(QFont::Monospace);
        le->setFont(mono);
        connect(le, &QLineEdit::textEdited, this, [apply](const QString& v) { apply(v); });
        return le;
    }
    case ParamKind::Color: {
        auto* b = new QPushButton;
        styleSwatch(b, cur.toList());
        connect(b, &QPushButton::clicked, this, [this, b, apply, cur] {
            const QVariantList c = cur.toList();
            QColor start = QColor::fromRgbF(c.value(0).toDouble(), c.value(1).toDouble(), c.value(2).toDouble(),
                                            c.value(3, 1.0).toDouble());
            const QColor picked = QColorDialog::getColor(start, this, "Pick colour", QColorDialog::ShowAlphaChannel);
            if (!picked.isValid()) return;
            const QVariantList v{picked.redF(), picked.greenF(), picked.blueF(), picked.alphaF()};
            apply(v);
            styleSwatch(b, v);
        });
        return b;
    }
    }
    return new QWidget;
}

QWidget* InspectorWidget::makeDefaultEditor(int id, int ii)
{
    const PortDef port = m_model->def(id)->inputs[ii];
    if (!port.defaultExpr.isEmpty())
        return note(port.defaultExpr.startsWith(QLatin1String("surface.")) ? "uses the Material panel when unconnected"
                                                            : "uses mesh data when unconnected");

    const QString name = port.name;
    const QVariantList cur = m_model->inputDefault(id, ii);
    const int n = (port.type == PortType::Generic) ? 1 : std::max(1, dimension(port.type));
    const QString mergeKey = QString("default:%1:%2").arg(id).arg(name);

    if (port.type == PortType::Bool) {
        auto* cb = new QCheckBox;
        cb->setChecked(!cur.isEmpty() && cur[0].toDouble() != 0.0);
        connect(cb, &QCheckBox::toggled, this, [this, id, name, mergeKey](bool v) {
            m_internalEdit = true;
            m_model->edit("Change default", [&] { m_model->setInputDefault(id, name, {v ? 1.0 : 0.0}); }, mergeKey);
            m_internalEdit = false;
        });
        return cb;
    }

    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(3);
    auto spinners = std::make_shared<QList<QDoubleSpinBox*>>();
    for (int k = 0; k < n; ++k) {
        auto* sb = new QDoubleSpinBox;
        sb->setDecimals(port.type == PortType::Int ? 0 : 4);
        sb->setRange(-1e6, 1e6);
        sb->setSingleStep(port.type == PortType::Int ? 1.0 : 0.05);
        sb->setValue(k < cur.size() ? cur[k].toDouble() : (cur.isEmpty() ? 0.0 : cur[0].toDouble()));
        sb->setMinimumWidth(52);
        h->addWidget(sb);
        spinners->append(sb);
    }
    for (QDoubleSpinBox* sb : *spinners) {
        connect(sb, &QDoubleSpinBox::valueChanged, this, [this, id, name, mergeKey, spinners](double) {
            QVariantList v;
            for (QDoubleSpinBox* s : *spinners) v << s->value();
            m_internalEdit = true;
            m_model->edit("Change default", [&] { m_model->setInputDefault(id, name, v); }, mergeKey);
            m_internalEdit = false;
        });
    }
    return row;
}

} // namespace sne
