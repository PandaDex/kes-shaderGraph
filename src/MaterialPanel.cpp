#include "MaterialPanel.h"

#include "GraphModel.h"
#include "KesProject.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace sne {

namespace {

QColor toColor(const double* c, double alpha)
{
    auto unit = [](double v) { return std::clamp(v, 0.0, 1.0); };
    return QColor::fromRgbF(float(unit(c[0])), float(unit(c[1])), float(unit(c[2])), float(unit(alpha)));
}

void styleSwatch(QPushButton* button, const QColor& color)
{
    button->setText(color.name(QColor::HexRgb));
    const QString text = color.lightnessF() > 0.5 ? QStringLiteral("#000000") : QStringLiteral("#ffffff");
    button->setStyleSheet(QString("background:%1; color:%2;").arg(color.name(QColor::HexRgb), text));
}

} // namespace

MaterialPanel::MaterialPanel(GraphModel* model, QWidget* parent) : QWidget(parent), m_model(model)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    root->addLayout(form);
    root->addStretch(1);

    m_baseColor = new QPushButton;
    m_emissive = new QPushButton;
    connect(m_baseColor, &QPushButton::clicked, this, [this] { pickColor(m_baseColor, true); });
    connect(m_emissive, &QPushButton::clicked, this, [this] { pickColor(m_emissive, false); });
    form->addRow("Base Color", m_baseColor);
    addNumber(form, "Metallic", 0.0, 1.0, &MaterialSettings::metallic);
    addNumber(form, "Roughness", 0.02, 1.0, &MaterialSettings::roughness);
    form->addRow("Emissive", m_emissive);
    addNumber(form, "Emissive Intensity", 0.0, 1000.0, &MaterialSettings::emissiveIntensity);
    addNumber(form, "Normal Scale", 0.0, 10.0, &MaterialSettings::normalScale);
    addNumber(form, "Occlusion Strength", 0.0, 1.0, &MaterialSettings::occlusionStrength);
    addNumber(form, "Alpha Cutoff", 0.0, 1.0, &MaterialSettings::alphaCutoff);
    addFlag(form, "Lit", &MaterialSettings::lit);
    addFlag(form, "Transparent", &MaterialSettings::transparent);
    addFlag(form, "Double Sided", &MaterialSettings::doubleSided);
    addFlag(form, "Cast Shadows", &MaterialSettings::castShadows);

    for (int slot = 0; slot < MaterialSlotCount; ++slot) {
        auto* row = new QWidget;
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* edit = new QLineEdit;
        edit->setPlaceholderText("none");
        auto* browse = new QToolButton;
        browse->setText("…");
        layout->addWidget(edit);
        layout->addWidget(browse);
        m_textures[slot] = edit;

        connect(browse, &QToolButton::clicked, this, [this, slot] { browseTexture(slot); });
        connect(edit, &QLineEdit::editingFinished, this, [this, slot, edit] {
            const QString text = edit->text().trimmed();
            apply(QString(), [slot, text](MaterialSettings& m) { m.textures[slot] = text; });
        });
        form->addRow(QString::fromLatin1(materialSlotInfo(slot).label), row);
    }

    connect(m_model, &GraphModel::materialChanged, this, &MaterialPanel::refresh);
    refresh();
}

QDoubleSpinBox* MaterialPanel::addNumber(QFormLayout* form, const QString& label, double min, double max,
                                         double MaterialSettings::*field)
{
    auto* spin = new QDoubleSpinBox;
    spin->setDecimals(3);
    spin->setRange(min, max);
    spin->setSingleStep(0.05);
    form->addRow(label, spin);
    connect(spin, &QDoubleSpinBox::valueChanged, this, [this, label, field](double v) {
        apply("material:" + label, [field, v](MaterialSettings& m) { m.*field = v; });
    });
    m_refreshers.push_back([this, spin, field] {
        const QSignalBlocker block(spin);
        spin->setValue(m_model->material().*field);
    });
    return spin;
}

QCheckBox* MaterialPanel::addFlag(QFormLayout* form, const QString& label, bool MaterialSettings::*field)
{
    auto* check = new QCheckBox;
    form->addRow(label, check);
    connect(check, &QCheckBox::toggled, this, [this, field](bool v) {
        apply(QString(), [field, v](MaterialSettings& m) { m.*field = v; });
    });
    m_refreshers.push_back([this, check, field] {
        const QSignalBlocker block(check);
        check->setChecked(m_model->material().*field);
    });
    return check;
}

void MaterialPanel::apply(const QString& key, const Change& change)
{
    MaterialSettings next = m_model->material();
    change(next);
    m_model->edit("Change material", [&] { m_model->setMaterial(next); }, key);
}

void MaterialPanel::refresh()
{
    const MaterialSettings& m = m_model->material();
    styleSwatch(m_baseColor, toColor(m.baseColor.data(), 1.0));
    styleSwatch(m_emissive, toColor(m.emissive.data(), 1.0));
    for (int i = 0; i < MaterialSlotCount; ++i) {
        const QSignalBlocker block(m_textures[i]);
        m_textures[i]->setText(m.textures[i]);
    }
    for (const auto& r : m_refreshers) r();
}

void MaterialPanel::pickColor(QPushButton* button, bool withAlpha)
{
    const MaterialSettings& m = m_model->material();
    const QColor start = withAlpha ? toColor(m.baseColor.data(), m.baseColor[3]) : toColor(m.emissive.data(), 1.0);
    QColorDialog::ColorDialogOptions options;
    if (withAlpha) options |= QColorDialog::ShowAlphaChannel;
    const QColor picked = QColorDialog::getColor(start, this, "Pick colour", options);
    if (!picked.isValid()) return;
    const double r = picked.redF(), g = picked.greenF(), b = picked.blueF(), a = picked.alphaF();
    apply(QString(), [=](MaterialSettings& s) {
        if (withAlpha) s.baseColor = {r, g, b, a};
        else s.emissive = {r, g, b};
    });
    styleSwatch(button, picked);
}

void MaterialPanel::browseTexture(int slot)
{
    const QString path = QFileDialog::getOpenFileName(this, "Choose texture", m_contentRoot,
                                                      "Images (*.png *.jpg *.jpeg *.bmp *.tga *.ktex);;All files (*)");
    if (path.isEmpty()) return;

    if (m_contentRoot.isEmpty()) {
        m_contentRoot = findContentRoot(path);
        if (!m_contentRoot.isEmpty()) emit contentRootFound(m_contentRoot);
    }
    const QString relative = contentRelativePath(m_contentRoot, path);
    if (relative.isEmpty()) {
        QMessageBox::warning(this, "Texture outside content",
                             "Materials reference textures relative to the content folder. "
                             "Open the project (File > Open Project) and pick a texture inside it.");
        return;
    }
    apply(QString(), [slot, relative](MaterialSettings& m) { m.textures[slot] = relative; });
}

} // namespace sne
