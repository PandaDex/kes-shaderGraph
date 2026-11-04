#pragma once

#include "MaterialSettings.h"

#include <QWidget>
#include <array>
#include <functional>
#include <vector>

class QCheckBox;
class QDoubleSpinBox;
class QFormLayout;
class QLineEdit;
class QPushButton;

namespace sne {

class GraphModel;

// Edits the .mat values of the current graph.
class MaterialPanel : public QWidget {
    Q_OBJECT
public:
    explicit MaterialPanel(GraphModel* model, QWidget* parent = nullptr);

    void setContentRoot(const QString& root) { m_contentRoot = root; }

signals:
    void contentRootFound(const QString& root);

private:
    using Change = std::function<void(MaterialSettings&)>;

    void refresh();
    void apply(const QString& key, const Change& change);
    QDoubleSpinBox* addNumber(QFormLayout* form, const QString& label, double min, double max,
                              double MaterialSettings::*field);
    QCheckBox* addFlag(QFormLayout* form, const QString& label, bool MaterialSettings::*field);
    void pickColor(QPushButton* button, bool withAlpha);
    void browseTexture(int slot);

    GraphModel* m_model;
    QString m_contentRoot;
    QPushButton* m_baseColor = nullptr;
    QPushButton* m_emissive = nullptr;
    std::array<QLineEdit*, MaterialSlotCount> m_textures{};
    std::vector<std::function<void()>> m_refreshers;
};

} // namespace sne
