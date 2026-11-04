#include "MainWindow.h"

#include "ExampleGraphs.h"
#include "GraphModel.h"
#include "InspectorWidget.h"
#include "NodeGraphScene.h"
#include "NodeGraphView.h"
#include "NodeSearchPopup.h"
#include "SlangHighlighter.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>

namespace sne {

namespace {

class LibraryTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

protected:
    QStringList mimeTypes() const override { return {NodeGraphView::kNodeMime}; }
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override
    {
        auto* m = new QMimeData;
        for (QTreeWidgetItem* it : items) {
            const QString type = it->data(0, Qt::UserRole).toString();
            if (!type.isEmpty()) { m->setData(NodeGraphView::kNodeMime, type.toUtf8()); break; }
        }
        return m;
    }
};

QPlainTextEdit* makeCodeView(bool readOnly)
{
    auto* e = new QPlainTextEdit;
    e->setReadOnly(readOnly);
    e->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont f("monospace");
    f.setStyleHint(QFont::Monospace);
    f.setPointSizeF(9.5);
    e->setFont(f);
    e->setTabStopDistance(QFontMetricsF(f).horizontalAdvance(' ') * 4);
    return e;
}

} // namespace

MainWindow::MainWindow()
{
    m_model = new GraphModel(this);
    m_scene = new NodeGraphScene(m_model, this);
    m_view = new NodeGraphView(m_scene, this);
    m_inspector = new InspectorWidget(m_model, this);
    m_popup = new NodeSearchPopup(this);

    buildUi();

    m_genTimer.setSingleShot(true);
    m_genTimer.setInterval(120);
    connect(&m_genTimer, &QTimer::timeout, this, &MainWindow::regenerate);

    connect(m_model, &GraphModel::changed, this, [this] {
        m_genTimer.start();
        updateStatus();
    });
    connect(m_model->undoStack(), &QUndoStack::cleanChanged, this, [this] { updateTitle(); });

    connect(m_scene, &NodeGraphScene::selectionIdsChanged, m_inspector, &InspectorWidget::setSelection);
    connect(m_scene, &NodeGraphScene::statusMessage, this, [this](const QString& s) { statusBar()->showMessage(s, 4000); });
    connect(m_view, &NodeGraphView::zoomChanged, this,
            [this](double z) { m_zoomLabel->setText(QString("%1%").arg(qRound(z * 100))); });
    connect(m_view, &NodeGraphView::nodeDropped, this,
            [this](const QString& type, const QPointF& pos) { m_scene->addNodeAt(type, pos); });
    connect(m_view, &NodeGraphView::contextMenuRequestedAt, this, [this](const QPointF& scenePos, const QPoint& global) {
        m_popupPos = scenePos;
        m_pendingFromNode = -1;
        m_popup->popup(global);
    });
    connect(m_scene, &NodeGraphScene::nodePopupRequested, this,
            [this](const QPointF& scenePos, int fromNode, int fromPort, bool fromIsOutput) {
                m_popupPos = scenePos;
                m_pendingFromNode = fromNode;
                m_pendingFromPort = fromPort;
                m_pendingFromIsOutput = fromIsOutput;
                m_popup->popup(QCursor::pos());
            });
    connect(m_popup, &NodeSearchPopup::nodeChosen, this, [this](const QString& type) {
        if (m_pendingFromNode >= 0)
            m_scene->createNodeAndConnect(type, m_popupPos, m_pendingFromNode, m_pendingFromPort, m_pendingFromIsOutput);
        else
            m_scene->addNodeAt(type, m_popupPos);
        m_pendingFromNode = -1;
        m_view->setFocus();
    });

    m_model->undoStack()->clear();
    m_model->undoStack()->setClean();

    QSettings s;
    restoreGeometry(s.value("geometry").toByteArray());
    restoreState(s.value("windowState").toByteArray());

    regenerate();
    updateTitle();
    updateStatus();
    QTimer::singleShot(0, this, [this] { m_view->frameAll(); });
}

void MainWindow::buildUi()
{
    setCentralWidget(m_view);
    buildDocks();
    buildActions();
    buildMenus();

    m_infoLabel = new QLabel;
    m_zoomLabel = new QLabel("100%");
    m_genLabel = new QLabel;
    statusBar()->addPermanentWidget(m_genLabel);
    statusBar()->addPermanentWidget(m_infoLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
    resize(1500, 900);
}

void MainWindow::buildDocks()
{
    auto* libWidget = new QWidget;
    auto* libLay = new QVBoxLayout(libWidget);
    libLay->setContentsMargins(6, 6, 6, 6);
    m_librarySearch = new QLineEdit;
    m_librarySearch->setPlaceholderText("Filter nodes…");
    m_librarySearch->setClearButtonEnabled(true);
    m_library = new LibraryTree;
    m_library->setHeaderHidden(true);
    m_library->setDragEnabled(true);
    m_library->setDragDropMode(QAbstractItemView::DragOnly);
    m_library->setIndentation(14);
    libLay->addWidget(m_librarySearch);
    libLay->addWidget(m_library);
    libLay->addWidget(new QLabel("<small>Double-click or drag onto the canvas.</small>"));
    populateLibrary();
    connect(m_librarySearch, &QLineEdit::textChanged, this, &MainWindow::filterLibrary);
    connect(m_library, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it) {
        const QString type = it->data(0, Qt::UserRole).toString();
        if (!type.isEmpty()) m_scene->addNodeAt(type, m_view->viewCenterScene());
    });

    auto* libDock = new QDockWidget("Node Library", this);
    libDock->setObjectName("LibraryDock");
    libDock->setWidget(libWidget);
    addDockWidget(Qt::LeftDockWidgetArea, libDock);

    auto* inspDock = new QDockWidget("Inspector", this);
    inspDock->setObjectName("InspectorDock");
    inspDock->setWidget(m_inspector);
    addDockWidget(Qt::RightDockWidgetArea, inspDock);

    m_code = makeCodeView(true);
    m_highlighter = new SlangHighlighter(m_code->document());
    m_problems = new QListWidget;
    m_problems->setObjectName("problemsList");

    m_bottomTabs = new QTabWidget;
    m_bottomTabs->setDocumentMode(true);
    m_bottomTabs->addTab(m_code, "Slang Source");
    m_bottomTabs->addTab(m_problems, "Problems");

    auto* bottomDock = new QDockWidget("Shader", this);
    bottomDock->setObjectName("ShaderDock");
    bottomDock->setWidget(m_bottomTabs);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);
    resizeDocks({bottomDock}, {260}, Qt::Vertical);
    resizeDocks({libDock, inspDock}, {250, 300}, Qt::Horizontal);

    menuBar();
    m_dockToggles = {libDock->toggleViewAction(), inspDock->toggleViewAction(), bottomDock->toggleViewAction()};
}

void MainWindow::populateLibrary()
{
    const NodeLibrary& lib = NodeLibrary::instance();
    for (const QString& cat : lib.categories()) {
        auto* catItem = new QTreeWidgetItem(m_library, {cat});
        catItem->setFlags(Qt::ItemIsEnabled);
        QFont f = catItem->font(0);
        f.setBold(true);
        catItem->setFont(0, f);
        catItem->setForeground(0, categoryColor(cat).lighter(140));
        for (const NodeDef& d : lib.all()) {
            if (d.category != cat) continue;
            auto* it = new QTreeWidgetItem(catItem, {d.title});
            it->setData(0, Qt::UserRole, d.type);
            it->setToolTip(0, d.description);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        }
        catItem->setExpanded(true);
    }
}

void MainWindow::filterLibrary(const QString& text)
{
    const QStringList words = text.toLower().split(' ', Qt::SkipEmptyParts);
    for (int c = 0; c < m_library->topLevelItemCount(); ++c) {
        QTreeWidgetItem* cat = m_library->topLevelItem(c);
        int visible = 0;
        for (int i = 0; i < cat->childCount(); ++i) {
            QTreeWidgetItem* it = cat->child(i);
            const QString hay = (it->text(0) + " " + cat->text(0) + " " + it->toolTip(0)).toLower();
            bool match = true;
            for (const QString& w : words) if (!hay.contains(w)) { match = false; break; }
            it->setHidden(!match);
            if (match) ++visible;
        }
        cat->setHidden(visible == 0);
        if (!words.isEmpty()) cat->setExpanded(true);
    }
}

void MainWindow::buildActions()
{
    auto make = [this](const QString& text, const QKeySequence& ks, auto slot) {
        auto* a = new QAction(text, this);
        if (!ks.isEmpty()) a->setShortcut(ks);
        connect(a, &QAction::triggered, this, slot);
        addAction(a);
        return a;
    };

    m_actNew = make("&New", QKeySequence::New, [this] { newGraph(); });
    m_actOpen = make("&Open…", QKeySequence::Open, [this] { openDialog(); });
    m_actExample = make("Open E&xample Graph", {}, [this] { loadExample(); });
    m_actSave = make("&Save", QKeySequence::Save, [this] { save(); });
    m_actSaveAs = make("Save &As…", QKeySequence::SaveAs, [this] { saveAs(); });
    m_actExport = make("&Export Slang…", QKeySequence("Ctrl+E"), [this] { exportSlang(); });
    m_actCopyCode = make("Copy Slang to &Clipboard", QKeySequence("Ctrl+Shift+C"), [this] { copySlang(); });
    m_actQuit = make("&Quit", QKeySequence::Quit, [this] { close(); });

    m_actUndo = m_model->undoStack()->createUndoAction(this, "Undo");
    m_actUndo->setShortcut(QKeySequence::Undo);
    m_actRedo = m_model->undoStack()->createRedoAction(this, "Redo");
    m_actRedo->setShortcuts({QKeySequence::Redo, QKeySequence("Ctrl+Y")});
    addAction(m_actUndo);
    addAction(m_actRedo);

    m_actCut = make("Cu&t", QKeySequence::Cut, [this] { m_scene->cutSelection(); });
    m_actCopy = make("&Copy", QKeySequence::Copy, [this] { m_scene->copySelection(); });
    m_actPaste = make("&Paste", QKeySequence::Paste, [this] { m_scene->paste(m_view->cursorScenePos()); });
    m_actDuplicate = make("&Duplicate", QKeySequence("Ctrl+D"), [this] { m_scene->duplicateSelection(); });
    m_actDelete = make("&Delete", QKeySequence::Delete, [this] { m_scene->deleteSelection(); });
    auto* backspace = new QShortcut(QKeySequence(Qt::Key_Backspace), m_view, [this] { m_scene->deleteSelection(); });
    backspace->setContext(Qt::WidgetWithChildrenShortcut);
    m_actSelectAll = make("Select &All", QKeySequence::SelectAll, [this] { m_scene->selectNodes(m_model->nodeIds()); });
    m_actAddNode = make("&Add Node…", QKeySequence("Shift+A"), [this] { showAddNodePopup(); });
    auto* tab = new QShortcut(QKeySequence(Qt::Key_Tab), m_view, [this] { showAddNodePopup(); });
    tab->setContext(Qt::WidgetShortcut);

    m_actFrameAll = make("Frame &All", QKeySequence(Qt::Key_F), [this] { m_view->frameAll(); });
    m_actFrameSel = make("Frame &Selection", QKeySequence("Shift+F"), [this] { m_view->frameSelection(); });
    m_actZoomIn = make("Zoom &In", QKeySequence::ZoomIn, [this] { m_view->zoomBy(1.2); });
    m_actZoomOut = make("Zoom &Out", QKeySequence::ZoomOut, [this] { m_view->zoomBy(1 / 1.2); });
    m_actZoomReset = make("&Reset Zoom", QKeySequence("Ctrl+0"), [this] { m_view->resetZoom(); });
    m_actSnap = make("Snap to &Grid", {}, [this] { m_scene->setSnapToGrid(m_actSnap->isChecked()); });
    m_actSnap->setCheckable(true);

    m_actAbout = make("&About", {}, [this] { about(); });

    auto* tb = addToolBar("Main");
    tb->setObjectName("MainToolBar");
    tb->setMovable(false);
    tb->addAction(m_actNew);
    tb->addAction(m_actOpen);
    tb->addAction(m_actSave);
    tb->addSeparator();
    tb->addAction(m_actUndo);
    tb->addAction(m_actRedo);
    tb->addSeparator();
    tb->addAction(m_actAddNode);
    tb->addAction(m_actFrameAll);
    tb->addSeparator();
    tb->addAction(m_actExport);
}

void MainWindow::buildMenus()
{
    QMenu* file = menuBar()->addMenu("&File");
    file->addAction(m_actNew);
    file->addAction(m_actOpen);
    file->addAction(m_actExample);
    file->addSeparator();
    file->addAction(m_actSave);
    file->addAction(m_actSaveAs);
    file->addSeparator();
    file->addAction(m_actExport);
    file->addAction(m_actCopyCode);
    file->addSeparator();
    file->addAction(m_actQuit);

    QMenu* edit = menuBar()->addMenu("&Edit");
    edit->addAction(m_actUndo);
    edit->addAction(m_actRedo);
    edit->addSeparator();
    edit->addAction(m_actCut);
    edit->addAction(m_actCopy);
    edit->addAction(m_actPaste);
    edit->addAction(m_actDuplicate);
    edit->addAction(m_actDelete);
    edit->addSeparator();
    edit->addAction(m_actSelectAll);
    edit->addAction(m_actAddNode);

    QMenu* view = menuBar()->addMenu("&View");
    view->addAction(m_actFrameAll);
    view->addAction(m_actFrameSel);
    view->addAction(m_actZoomIn);
    view->addAction(m_actZoomOut);
    view->addAction(m_actZoomReset);
    view->addSeparator();
    view->addAction(m_actSnap);
    view->addSeparator();
    for (QAction* a : m_dockToggles) view->addAction(a);

    QMenu* help = menuBar()->addMenu("&Help");
    help->addAction(m_actAbout);
}

QString MainWindow::graphName() const
{
    return m_currentFile.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(m_currentFile).completeBaseName();
}

void MainWindow::updateTitle()
{
    setWindowTitle(QString("%1[*] — ShaderGraph Editor").arg(graphName()));
    setWindowModified(!m_model->undoStack()->isClean());
}

void MainWindow::updateStatus()
{
    m_infoLabel->setText(QString("  %1 nodes · %2 connections  ").arg(m_model->nodeCount()).arg(m_model->connections().size()));
}

bool MainWindow::maybeSave()
{
    if (m_model->undoStack()->isClean()) return true;
    const auto r = QMessageBox::question(this, "Unsaved changes",
                                         QString("Save changes to \"%1\" before continuing?").arg(graphName()),
                                         QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Save) return save();
    return r == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (!maybeSave()) { e->ignore(); return; }
    QSettings s;
    s.setValue("geometry", saveGeometry());
    s.setValue("windowState", saveState());
    e->accept();
}

void MainWindow::newGraph()
{
    if (!maybeSave()) return;
    buildEmptyGraph(*m_model);
    m_model->undoStack()->clear();
    m_model->undoStack()->setClean();
    m_currentFile.clear();
    updateTitle();
    m_view->frameAll();
}

void MainWindow::loadExample()
{
    if (!maybeSave()) return;
    buildExampleGraph(*m_model);
    m_model->undoStack()->clear();
    m_model->undoStack()->setClean();
    m_currentFile.clear();
    updateTitle();
    m_view->frameAll();
}

void MainWindow::openDialog()
{
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Open graph", QString(),
                                                      "Slang node graphs (*.slangnodes *.json);;All files (*)");
    if (!path.isEmpty()) openFile(path);
}

bool MainWindow::openFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Open failed", QString("Could not read %1:\n%2").arg(path, f.errorString()));
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (!doc.isObject()) {
        QMessageBox::warning(this, "Open failed", QString("%1 is not a valid graph file.\n%2").arg(path, err.errorString()));
        return false;
    }
    QStringList warnings;
    if (!m_model->loadJson(doc.object(), &warnings)) {
        QMessageBox::warning(this, "Open failed", warnings.join('\n'));
        return false;
    }
    m_model->undoStack()->clear();
    m_model->undoStack()->setClean();
    m_currentFile = path;
    updateTitle();
    m_view->frameAll();
    if (!warnings.isEmpty()) QMessageBox::information(this, "Loaded with warnings", warnings.join('\n'));
    statusBar()->showMessage("Opened " + path, 3000);
    return true;
}

bool MainWindow::saveTo(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Save failed", QString("Could not write %1:\n%2").arg(path, f.errorString()));
        return false;
    }
    f.write(QJsonDocument(m_model->toJson()).toJson(QJsonDocument::Indented));
    m_currentFile = path;
    m_model->undoStack()->setClean();
    updateTitle();
    statusBar()->showMessage("Saved " + path, 3000);
    return true;
}

bool MainWindow::save()
{
    return m_currentFile.isEmpty() ? saveAs() : saveTo(m_currentFile);
}

bool MainWindow::saveAs()
{
    QString path = QFileDialog::getSaveFileName(this, "Save graph", graphName() + ".slangnodes",
                                                "Slang node graphs (*.slangnodes);;JSON (*.json)");
    if (path.isEmpty()) return false;
    if (QFileInfo(path).suffix().isEmpty()) path += ".slangnodes";
    return saveTo(path);
}

void MainWindow::exportSlang()
{
    regenerate();
    if (!m_gen.ok) {
        QMessageBox::warning(this, "Cannot export", m_gen.errors.join('\n'));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, "Export Slang shader", graphName() + ".slang",
                                                      "Slang shaders (*.slang)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Export failed", f.errorString());
        return;
    }
    f.write(m_gen.code.toUtf8());
    statusBar()->showMessage("Exported " + path, 3000);
}

void MainWindow::copySlang()
{
    regenerate();
    QApplication::clipboard()->setText(m_gen.code);
    statusBar()->showMessage("Slang source copied to clipboard", 2500);
}

void MainWindow::regenerate()
{
    m_genTimer.stop();
    m_gen = SlangCodeGen::generate(*m_model, graphName());

    const int scroll = m_code->verticalScrollBar()->value();
    m_code->setPlainText(m_gen.ok ? m_gen.code : QString("// Cannot generate shader:\n// %1\n").arg(m_gen.errors.join("\n// ")));
    m_code->verticalScrollBar()->setValue(scroll);

    m_genLabel->setText(m_gen.ok ? QString() : QStringLiteral("⚠ shader has errors"));
    refreshProblems();
}

void MainWindow::refreshProblems()
{
    m_problems->clear();
    auto add = [&](bool error, const QString& text) {
        auto* it = new QListWidgetItem((error ? "✖ " : "⚠ ") + text);
        it->setForeground(error ? QColor("#ff7b72") : QColor("#e3b341"));
        m_problems->addItem(it);
    };
    for (const QString& e : m_gen.errors) add(true, e);
    for (const QString& w : m_gen.warnings) add(false, w);

    const int n = m_problems->count();
    m_bottomTabs->setTabText(1, n ? QString("Problems (%1)").arg(n) : "Problems");
}

void MainWindow::showAddNodePopup()
{
    m_popupPos = m_view->cursorScenePos();
    m_pendingFromNode = -1;
    const QPoint local = m_view->viewport()->mapFromGlobal(QCursor::pos());
    const QPoint global = m_view->viewport()->rect().contains(local)
                              ? QCursor::pos()
                              : m_view->viewport()->mapToGlobal(m_view->viewport()->rect().center());
    m_popup->popup(global);
}

void MainWindow::about()
{
    QMessageBox::about(this, "About ShaderGraph Editor","Node based editor for slang shaders" 
        "This application is part of <b>KES Engine</b> project.");
}

} // namespace sne
