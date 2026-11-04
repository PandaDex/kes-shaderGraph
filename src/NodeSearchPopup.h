#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace sne {

class NodeSearchPopup : public QWidget {
    Q_OBJECT
public:
    explicit NodeSearchPopup(QWidget* parent = nullptr);

    void popup(const QPoint& globalPos);

signals:
    void nodeChosen(const QString& type);

protected:
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void refill(const QString& filter);
    void accept(QListWidgetItem* item);

    QLineEdit* m_edit;
    QListWidget* m_list;
};

} // namespace sne
