#pragma once

#include "GraphItems.h"
#include "SlangCodeGen.h"

#include <QList>
#include <QMainWindow>
#include <QTimer>
#include <array>

class QAction;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTabWidget;
class QTreeWidget;

namespace sne {

class GraphModel;
class InspectorWidget;
class MaterialPanel;
class NodeGraphScene;
class NodeGraphView;
class NodeSearchPopup;
class PreviewCompiler;
class PreviewWidget;
class SlangHighlighter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

    bool openFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void newGraph();
    void openDialog();
    void loadExample();
    bool save();
    bool saveAs();
    void exportSlang();
    void copySlang();
    void exportMaterial();
    void openProject();
    void locateSlangc();

    void regenerate();
    void showAddNodePopup();
    void about();

private:
    void buildUi();
    void buildActions();
    void buildMenus();
    void buildDocks();
    void populateLibrary();
    void filterLibrary(const QString& text);
    void refreshProblems();
    void updateTitle();
    void updateStatus();
    bool maybeSave();
    bool saveTo(const QString& path);
    QString graphName() const;
    QWidget* buildPreviewPanel();
    void setContentRoot(const QString& root);
    void setPreviewErrors(const QString& log);
    void syncMaterial();
    void detectSlangc();

    GraphModel* m_model;
    NodeGraphScene* m_scene;
    NodeGraphView* m_view;
    InspectorWidget* m_inspector;
    NodeSearchPopup* m_popup;
    MaterialPanel* m_materialPanel;
    PreviewWidget* m_preview;
    PreviewCompiler* m_compiler;

    QPlainTextEdit* m_code;
    QListWidget* m_problems;
    QTabWidget* m_bottomTabs;
    QTreeWidget* m_library;
    QLineEdit* m_librarySearch;
    SlangHighlighter* m_highlighter;

    QLabel* m_infoLabel;
    QLabel* m_zoomLabel;
    QLabel* m_genLabel;
    QLabel* m_previewStatus = nullptr;
    QComboBox* m_targetCombo = nullptr;

    QAction *m_actNew = nullptr, *m_actOpen = nullptr, *m_actExample = nullptr, *m_actSave = nullptr,
            *m_actSaveAs = nullptr, *m_actExport = nullptr, *m_actCopyCode = nullptr, *m_actQuit = nullptr;
    QAction *m_actUndo = nullptr, *m_actRedo = nullptr, *m_actCut = nullptr, *m_actCopy = nullptr,
            *m_actPaste = nullptr, *m_actDuplicate = nullptr, *m_actDelete = nullptr,
            *m_actSelectAll = nullptr, *m_actAddNode = nullptr;
    QAction *m_actFrameAll = nullptr, *m_actFrameSel = nullptr, *m_actZoomIn = nullptr, *m_actZoomOut = nullptr,
            *m_actZoomReset = nullptr, *m_actSnap = nullptr;
    QAction* m_actAbout = nullptr;
    QAction *m_actExportMaterial = nullptr, *m_actOpenProject = nullptr, *m_actLocateSlangc = nullptr;
    QList<QAction*> m_dockToggles;

    QTimer m_genTimer;
    CodeGenResult m_gen;
    ShaderTarget m_target = ShaderTarget::KesEngine;
    QString m_currentFile;

    QString m_contentRoot;
    QString m_projectPath;
    QString m_previewSource;
    QStringList m_previewErrors;
    std::array<QString, MaterialSlotCount> m_textureProblems;
    std::array<QString, MaterialSlotCount> m_loadedTextures;

    QPointF m_popupPos;
    int m_pendingFromNode = -1;
    int m_pendingFromPort = -1;
    bool m_pendingFromIsOutput = false;
};

} // namespace sne
