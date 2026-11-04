#pragma once

#include <QList>
#include <QWidget>

class QVBoxLayout;

namespace sne {

class GraphModel;

class InspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit InspectorWidget(GraphModel* model, QWidget* parent = nullptr);

    void setSelection(const QList<int>& ids);

private slots:
    void scheduleRebuild();
    void rebuild();

private:
    QWidget* makeParamEditor(int nodeId, int paramIndex);
    QWidget* makeDefaultEditor(int nodeId, int inputIndex);

    GraphModel* m_model;
    QVBoxLayout* m_root;
    QWidget* m_content = nullptr;
    QList<int> m_selection;
    bool m_internalEdit = false;
    bool m_rebuildPending = false;
};

} // namespace sne
