#include "MainWindow.hpp"
#include "ImageProcessor.hpp"
#include "FileManager.hpp"
#include "FilterDialog.hpp"
#include "CompareView.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QSplitter>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollBar>
#include <QStyle>
#include <QFontMetrics>
#include <QHeaderView>
#include <QMenuBar>
#include <QDir>
#include <algorithm>
#include <cmath>

// ── Constructor / Destructor ──────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("CImg Viewer");
    setMinimumSize(900, 650);
    setAcceptDrops(true);

    // Central widget: scroll area + image label
    m_imageLabel = new ImageLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_imageLabel->setBackgroundRole(QPalette::Dark);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setBackgroundRole(QPalette::Dark);

    // Compare view — hidden until compare mode is activated
    m_compareView = new CompareView(this);
    m_compareView->hide();

    // Use the scroll area as the default central widget
    setCentralWidget(m_scrollArea);

    createFileBrowserDock();
    createHistogramDock();
    createActions();
    createMenus();
    createToolBar();
    createStatusBarWidgets();

    m_imageLabel->setScrollArea(m_scrollArea);   // enable pan
    connect(m_imageLabel, &ImageLabel::cropSelected, this, &MainWindow::performCrop);

    loadSettings();
    updateActions();
}

MainWindow::~MainWindow() {}

// ── Dock: file browser ───────────────────────────────────────────────────────

void MainWindow::createFileBrowserDock() {
    m_fileDock = new QDockWidget("File Browser", this);
    m_fileDock->setObjectName("fileDock");

    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setRootPath(QDir::homePath());
    m_fsModel->setNameFilters({"*.jpg","*.jpeg","*.png","*.bmp","*.tiff","*.tif",
                                "*.pbm","*.pgm","*.ppm"});
    m_fsModel->setNameFilterDisables(false); // hide non-matching files

    m_fileTree = new QTreeView(this);
    m_fileTree->setModel(m_fsModel);
    m_fileTree->setRootIndex(m_fsModel->index(QDir::homePath()));
    m_fileTree->setColumnHidden(1, true);
    m_fileTree->setColumnHidden(2, true);
    m_fileTree->setColumnHidden(3, true);
    m_fileTree->header()->hide();
    m_fileTree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    connect(m_fileTree, &QTreeView::clicked, this, &MainWindow::onFileBrowserClicked);

    m_fileDock->setWidget(m_fileTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileDock);
}

// ── Dock: histogram ──────────────────────────────────────────────────────────

void MainWindow::createHistogramDock() {
    m_histDock = new QDockWidget("Histogram", this);
    m_histDock->setObjectName("histDock");

    m_histWidget = new HistogramWidget(this);
    m_histDock->setWidget(m_histWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_histDock);
}

// ── Actions ──────────────────────────────────────────────────────────────────

void MainWindow::createActions() {
    // File
    m_actOpen = new QAction(QIcon::fromTheme("document-open"), "&Open…", this);
    m_actOpen->setShortcut(QKeySequence::Open);
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::onOpenFile);

    m_actOpenFolder = new QAction(QIcon::fromTheme("folder-open"), "Open &Folder…", this);
    m_actOpenFolder->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_O);
    connect(m_actOpenFolder, &QAction::triggered, this, &MainWindow::onOpenFolder);

    m_actSave = new QAction(QIcon::fromTheme("document-save"), "&Save", this);
    m_actSave->setShortcut(QKeySequence::Save);
    connect(m_actSave, &QAction::triggered, this, &MainWindow::onSave);

    m_actSaveAs = new QAction(QIcon::fromTheme("document-save-as"), "Save &As…", this);
    m_actSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(m_actSaveAs, &QAction::triggered, this, &MainWindow::onSaveAs);

    // Edit
    m_actUndo = new QAction(QIcon::fromTheme("edit-undo"), "&Undo", this);
    m_actUndo->setShortcut(QKeySequence::Undo);
    connect(m_actUndo, &QAction::triggered, this, &MainWindow::onUndo);

    m_actRedo = new QAction(QIcon::fromTheme("edit-redo"), "&Redo", this);
    m_actRedo->setShortcut(QKeySequence::Redo);
    connect(m_actRedo, &QAction::triggered, this, &MainWindow::onRedo);

    // View
    m_actZoomIn = new QAction(QIcon::fromTheme("zoom-in"), "Zoom &In", this);
    m_actZoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(m_actZoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);

    m_actZoomOut = new QAction(QIcon::fromTheme("zoom-out"), "Zoom &Out", this);
    m_actZoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(m_actZoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);

    m_actFitWindow = new QAction(QIcon::fromTheme("zoom-fit-best"), "&Fit to Window", this);
    m_actFitWindow->setShortcut(Qt::Key_F);
    m_actFitWindow->setCheckable(true);
    connect(m_actFitWindow, &QAction::toggled, this, &MainWindow::onFitToWindow);

    m_actActualSize = new QAction(QIcon::fromTheme("zoom-original"), "Actual &Size (100%)", this);
    m_actActualSize->setShortcut(Qt::CTRL | Qt::Key_0);
    connect(m_actActualSize, &QAction::triggered, this, &MainWindow::onActualSize);

    // Folder navigation
    m_actPrev = new QAction(QIcon::fromTheme("go-previous"), "&Previous Image", this);
    m_actPrev->setShortcut(Qt::Key_Left);
    connect(m_actPrev, &QAction::triggered, this, &MainWindow::onPrevImage);
    m_imageActions.append(m_actPrev);

    m_actNext = new QAction(QIcon::fromTheme("go-next"), "&Next Image", this);
    m_actNext->setShortcut(Qt::Key_Right);
    connect(m_actNext, &QAction::triggered, this, &MainWindow::onNextImage);
    m_imageActions.append(m_actNext);

    // Slideshow
    m_actSlideshow = new QAction(QIcon::fromTheme("media-playback-start"), "S&lideshow", this);
    m_actSlideshow->setShortcut(Qt::Key_F5);
    m_actSlideshow->setCheckable(true);
    connect(m_actSlideshow, &QAction::toggled, this, &MainWindow::onSlideshow);
    m_imageActions.append(m_actSlideshow);

    m_slideshowTimer = new QTimer(this);
    connect(m_slideshowTimer, &QTimer::timeout, this, &MainWindow::onSlideshowTick);

    // Compare mode
    m_actCompare = new QAction("&Compare Mode", this);
    m_actCompare->setShortcut(Qt::Key_V);
    m_actCompare->setCheckable(true);
    m_actCompare->setToolTip("Side-by-side comparison (V)");
    connect(m_actCompare, &QAction::toggled, this, &MainWindow::onCompareMode);
    m_imageActions.append(m_actCompare);

    // Crop (toggle tool)
    m_actCrop = new QAction("&Crop", this);
    m_actCrop->setShortcut(Qt::Key_C);
    m_actCrop->setCheckable(true);
    connect(m_actCrop, &QAction::toggled, this, [this](bool on) {
        m_imageLabel->setTool(on ? ImageLabel::Tool::Crop : ImageLabel::Tool::None);
        if (!on) m_actCrop->setChecked(false);
    });
    connect(m_imageLabel, &ImageLabel::cropSelected, this, [this](QRect) {
        m_actCrop->setChecked(false);
    });
}

// ── Menus ────────────────────────────────────────────────────────────────────

void MainWindow::createMenus() {
    // ── File ──────────────────────────────────────────────────────────────────
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(m_actOpen);
    fileMenu->addAction(m_actOpenFolder);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actSaveAs);
    fileMenu->addSeparator();

    m_recentMenu = fileMenu->addMenu("&Recent Files");
    rebuildRecentMenu();

    fileMenu->addSeparator();
    QAction* actExit = fileMenu->addAction("E&xit", this, &QWidget::close);
    actExit->setShortcut(QKeySequence::Quit);

    // ── Edit ──────────────────────────────────────────────────────────────────
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(m_actUndo);
    editMenu->addAction(m_actRedo);

    // ── View ──────────────────────────────────────────────────────────────────
    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_actZoomIn);
    viewMenu->addAction(m_actZoomOut);
    viewMenu->addAction(m_actFitWindow);
    viewMenu->addAction(m_actActualSize);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actCompare);
    viewMenu->addAction(m_actSlideshow);
    viewMenu->addSeparator();
    viewMenu->addAction(m_fileDock->toggleViewAction());
    viewMenu->addAction(m_histDock->toggleViewAction());

    // ── Image ─────────────────────────────────────────────────────────────────
    QMenu* imgMenu = menuBar()->addMenu("&Image");

    // Adjustments
    QMenu* adjMenu = imgMenu->addMenu("&Adjustments");
    auto addAdj = [&](const QString& name, auto slot) {
        QAction* a = adjMenu->addAction(name, this, slot);
        m_imageActions.append(a);
        return a;
    };
    addAdj("&Brightness…",           &MainWindow::onBrightness);
    addAdj("&Contrast…",             &MainWindow::onContrast);
    addAdj("&Gamma Correction…",     &MainWindow::onGamma);
    adjMenu->addSeparator();
    addAdj("Convert to &Grayscale",  &MainWindow::onGrayscale);
    addAdj("&Negative (Invert)",     &MainWindow::onNegative);
    addAdj("&Histogram Equalization",&MainWindow::onHistogramEq);

    // Filters
    QMenu* filtMenu = imgMenu->addMenu("&Filters");
    auto addFilt = [&](const QString& name, auto slot) {
        QAction* a = filtMenu->addAction(name, this, slot);
        m_imageActions.append(a);
        return a;
    };
    addFilt("&Gaussian Blur…",  &MainWindow::onGaussianBlur);
    addFilt("&Median Blur…",    &MainWindow::onMedianBlur);
    addFilt("&Sharpen…",        &MainWindow::onSharpen);
    filtMenu->addSeparator();
    addFilt("&Sobel Edge",      &MainWindow::onSobel);
    addFilt("&Canny Edge…",     &MainWindow::onCanny);
    addFilt("&Emboss",          &MainWindow::onEmboss);

    // Transform
    QMenu* trMenu = imgMenu->addMenu("&Transform");
    auto addTr = [&](const QString& name, auto slot, QKeySequence sc = {}) {
        QAction* a = trMenu->addAction(name, this, slot);
        if (!sc.isEmpty()) a->setShortcut(sc);
        m_imageActions.append(a);
        return a;
    };
    addTr("Rotate &Right (CW)",      &MainWindow::onRotateCW,  Qt::CTRL | Qt::Key_BracketRight);
    addTr("Rotate &Left (CCW)",      &MainWindow::onRotateCCW, Qt::CTRL | Qt::Key_BracketLeft);
    addTr("Flip &Horizontal",        &MainWindow::onFlipH);
    addTr("Flip &Vertical",          &MainWindow::onFlipV);
    trMenu->addSeparator();
    m_imageActions.append(m_actCrop);
    trMenu->addAction(m_actCrop);
    addTr("&Resize…",                &MainWindow::onResize);

    // Morphology
    QMenu* morphMenu = imgMenu->addMenu("&Morphology");
    auto addMorph = [&](const QString& name, auto slot) {
        QAction* a = morphMenu->addAction(name, this, slot);
        m_imageActions.append(a);
        return a;
    };
    addMorph("&Erode…",            &MainWindow::onErode);
    addMorph("&Dilate…",           &MainWindow::onDilate);
    morphMenu->addSeparator();
    addMorph("M. &Open…",          &MainWindow::onMorphOpen);
    addMorph("M. &Close…",         &MainWindow::onMorphClose);
    addMorph("M. &Gradient…",      &MainWindow::onMorphGradient);

    // ── Help ──────────────────────────────────────────────────────────────────
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&Keyboard Shortcuts", this, &MainWindow::onKeyboardShortcuts);
    helpMenu->addSeparator();
    helpMenu->addAction("&About", this, &MainWindow::onAbout);
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

void MainWindow::createToolBar() {
    QToolBar* tb = addToolBar("Main");
    tb->setObjectName("mainToolBar");
    tb->setMovable(false);

    tb->addAction(m_actOpen);
    tb->addAction(m_actSave);
    tb->addSeparator();
    tb->addAction(m_actUndo);
    tb->addAction(m_actRedo);
    tb->addSeparator();
    tb->addAction(m_actZoomOut);
    tb->addAction(m_actZoomIn);
    tb->addAction(m_actFitWindow);
    tb->addAction(m_actActualSize);
    tb->addSeparator();

    // Rotate/flip quick buttons
    auto addTbBtn = [&](const QString& text, auto slot) {
        QAction* a = tb->addAction(text, this, slot);
        m_imageActions.append(a);
        return a;
    };
    addTbBtn("↻",  &MainWindow::onRotateCW)->setToolTip("Rotate CW");
    addTbBtn("↺",  &MainWindow::onRotateCCW)->setToolTip("Rotate CCW");
    addTbBtn("↔",  &MainWindow::onFlipH)->setToolTip("Flip Horizontal");
    addTbBtn("↕",  &MainWindow::onFlipV)->setToolTip("Flip Vertical");

    tb->addAction(m_actCompare);
    tb->addSeparator();
    tb->addAction(m_actPrev);
    tb->addAction(m_actNext);
    tb->addAction(m_actSlideshow);

    // Interval combo — visible only during slideshow
    m_slideshowCombo = new QComboBox(tb);
    m_slideshowCombo->addItem("2 s",  2000);
    m_slideshowCombo->addItem("5 s",  5000);
    m_slideshowCombo->addItem("10 s", 10000);
    m_slideshowCombo->addItem("30 s", 30000);
    m_slideshowCombo->setCurrentIndex(1); // 5 s default
    m_slideshowCombo->setVisible(false);
    m_slideshowCombo->setToolTip("Slideshow interval");
    connect(m_slideshowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (m_slideshowTimer->isActive()) {
                    int ms = m_slideshowCombo->currentData().toInt();
                    m_slideshowTimer->setInterval(ms);
                }
            });
    m_slideshowComboAction = tb->addWidget(m_slideshowCombo);
}

// ── Status bar ───────────────────────────────────────────────────────────────

void MainWindow::createStatusBarWidgets() {
    m_statusImgInfo = new QLabel(this);
    m_statusZoom    = new QLabel(this);
    m_statusPath    = new QLabel(this);
    m_statusPath->setTextFormat(Qt::PlainText);

    statusBar()->addWidget(m_statusImgInfo);
    statusBar()->addWidget(m_statusZoom);
    statusBar()->addWidget(m_statusPath, 1);
}

// ── Load / Display ───────────────────────────────────────────────────────────

void MainWindow::openPath(const QString& path) { loadImage(path); }

void MainWindow::loadImage(const QString& path) {
    QString err;
    CImgU8 img = FileManager::loadImage(path, &err);
    if (img.is_empty()) {
        QMessageBox::warning(this, "Open Error", err);
        return;
    }
    m_currentImage     = img;
    m_originalImage    = img;   // freeze for compare view
    m_currentFilePath  = path;

    m_undoStack.clear();
    m_undoStack.push("Load", m_currentImage);

    // Reset zoom when loading a new image
    m_fitToWindow = false;
    m_actFitWindow->setChecked(false);
    m_zoom = 1.0;

    displayImage();
    updateHistogram();
    updateActions();
    updateStatusBar();

    FileManager::addRecentFile(path);
    rebuildRecentMenu();

    setWindowTitle(QFileInfo(path).fileName() + " — CImg Viewer");
    buildFolderList(QFileInfo(path).absolutePath());
}

void MainWindow::displayImage() {
    if (m_currentImage.is_empty()) {
        m_imageLabel->clear();
        return;
    }

    QImage qi = ImageConvert::toQImage(m_currentImage);
    QPixmap pm = QPixmap::fromImage(qi);

    if (m_fitToWindow) {
        QSize vp = m_scrollArea->viewport()->size();
        QPixmap scaled = pm.scaled(vp, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_zoom = (double)scaled.width() / pm.width();
        m_imageLabel->setDisplayZoom(m_zoom);
        m_imageLabel->setPixmap(scaled);
        m_imageLabel->adjustSize();
    } else {
        if (m_zoom != 1.0) {
            pm = pm.scaled((int)std::round(pm.width()  * m_zoom),
                           (int)std::round(pm.height() * m_zoom),
                           Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
        }
        m_imageLabel->setDisplayZoom(m_zoom);
        m_imageLabel->setPixmap(pm);
        m_imageLabel->adjustSize();
    }
    updateStatusBar();
}

void MainWindow::applyOperation(const QString& name, const CImgU8& result) {
    m_currentImage = result;
    m_undoStack.push(name.toStdString(), m_currentImage);
    displayImage();
    updateHistogram();
    updateActions();
    // Keep compare view in sync
    if (m_actCompare->isChecked())
        m_compareView->setEdited(QPixmap::fromImage(ImageConvert::toQImage(m_currentImage)));
}

void MainWindow::updateStatusBar() {
    if (m_currentImage.is_empty()) {
        m_statusImgInfo->clear();
        m_statusZoom->clear();
        m_statusPath->clear();
        return;
    }
    m_statusImgInfo->setText(QString("%1 × %2  ch:%3")
        .arg(m_currentImage.width())
        .arg(m_currentImage.height())
        .arg(m_currentImage.spectrum()));
    m_statusZoom->setText(QString("  Zoom: %1%  ").arg((int)std::round(m_zoom * 100)));

    // Slideshow indicator
    if (m_slideshowTimer && m_slideshowTimer->isActive()) {
        int idx   = currentFolderIndex();
        int total = m_folderImages.size();
        m_statusPath->setText(
            QString("Slideshow — %1/%2  %3")
                .arg(idx + 1).arg(total).arg(m_currentFilePath));
    } else {
        m_statusPath->setText(m_currentFilePath);
    }
}

void MainWindow::updateActions() {
    bool has = !m_currentImage.is_empty();
    for (QAction* a : m_imageActions) a->setEnabled(has);
    m_actSave->setEnabled(has && !m_currentFilePath.isEmpty());
    m_actSaveAs->setEnabled(has);
    m_actZoomIn->setEnabled(has);
    m_actZoomOut->setEnabled(has);
    m_actFitWindow->setEnabled(has);
    m_actActualSize->setEnabled(has);

    m_actUndo->setEnabled(m_undoStack.canUndo());
    m_actRedo->setEnabled(m_undoStack.canRedo());

    QString uname = QString::fromStdString(m_undoStack.nextUndoName());
    QString rname = QString::fromStdString(m_undoStack.nextRedoName());
    m_actUndo->setText(uname.isEmpty() ? "&Undo" : "&Undo " + uname);
    m_actRedo->setText(rname.isEmpty() ? "&Redo" : "&Redo " + rname);
}

void MainWindow::updateHistogram() {
    if (m_currentImage.is_empty()) { m_histWidget->clear(); return; }
    auto hists = ImageProcessor::computeHistograms(m_currentImage);
    m_histWidget->setHistogram(hists, m_currentImage.spectrum() > 1);
}

void MainWindow::setZoom(double z) {
    m_zoom = std::clamp(z, 0.05, 16.0);
    m_fitToWindow = false;
    m_actFitWindow->setChecked(false);
    displayImage();
}

// ── File slots ────────────────────────────────────────────────────────────────

void MainWindow::onOpenFile() {
    QString path = QFileDialog::getOpenFileName(this, "Open Image", QString(),
                                                FileManager::imageFilter());
    if (!path.isEmpty()) loadImage(path);
}

void MainWindow::onOpenFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Open Folder",
                                                    QDir::homePath());
    if (dir.isEmpty()) return;
    m_fileTree->setRootIndex(m_fsModel->index(dir));
    m_fileDock->show();
    m_fileDock->raise();
}

void MainWindow::onSave() {
    if (m_currentFilePath.isEmpty()) { onSaveAs(); return; }
    QString err;
    if (!FileManager::saveImage(m_currentImage, m_currentFilePath, &err))
        QMessageBox::warning(this, "Save Error", err);
}

void MainWindow::onSaveAs() {
    QString path = QFileDialog::getSaveFileName(this, "Save Image As",
        m_currentFilePath.isEmpty() ? QDir::homePath() : m_currentFilePath,
        "Images (*.png *.jpg *.jpeg *.bmp *.tiff);;All files (*)");
    if (path.isEmpty()) return;
    QString err;
    if (FileManager::saveImage(m_currentImage, path, &err)) {
        m_currentFilePath = path;
        setWindowTitle(QFileInfo(path).fileName() + " — CImg Viewer");
        updateStatusBar();
        FileManager::addRecentFile(path);
        rebuildRecentMenu();
    } else {
        QMessageBox::warning(this, "Save Error", err);
    }
}

void MainWindow::onOpenRecent() {
    QAction* a = qobject_cast<QAction*>(sender());
    if (a) loadImage(a->data().toString());
}

void MainWindow::onClearRecent() {
    FileManager::clearRecentFiles();
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu() {
    m_recentMenu->clear();
    QStringList files = FileManager::recentFiles();
    for (const QString& f : files) {
        QAction* a = m_recentMenu->addAction(QFileInfo(f).fileName());
        a->setData(f);
        a->setToolTip(f);
        connect(a, &QAction::triggered, this, &MainWindow::onOpenRecent);
    }
    if (!files.isEmpty()) {
        m_recentMenu->addSeparator();
        m_recentMenu->addAction("&Clear Recent", this, &MainWindow::onClearRecent);
    }
    m_recentMenu->setEnabled(!files.isEmpty());
}

// ── Edit slots ────────────────────────────────────────────────────────────────

void MainWindow::onUndo() {
    if (!m_undoStack.canUndo()) return;
    m_currentImage = m_undoStack.undo();
    displayImage(); updateHistogram(); updateActions();
}

void MainWindow::onRedo() {
    if (!m_undoStack.canRedo()) return;
    m_currentImage = m_undoStack.redo();
    displayImage(); updateHistogram(); updateActions();
}

// ── View slots ────────────────────────────────────────────────────────────────

void MainWindow::onZoomIn()  { setZoom(m_zoom * 1.25); }
void MainWindow::onZoomOut() { setZoom(m_zoom / 1.25); }

void MainWindow::onFitToWindow(bool checked) {
    m_fitToWindow = checked;
    displayImage();
}

void MainWindow::onActualSize() { setZoom(1.0); }

// ── Adjustment slots ──────────────────────────────────────────────────────────

void MainWindow::onBrightness() {
    FilterDialog::Param p{"Delta", -100, 100, 0, 200, true, ""};
    FilterDialog dlg("Brightness", {p}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::brightness(img, (int)v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Brightness", ImageProcessor::brightness(m_currentImage, (int)dlg.values()[0]));
}

void MainWindow::onContrast() {
    FilterDialog::Param p{"Factor", 0.1, 3.0, 1.0, 290, false, "×"};
    FilterDialog dlg("Contrast", {p}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::contrast(img, v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Contrast", ImageProcessor::contrast(m_currentImage, dlg.values()[0]));
}

void MainWindow::onGamma() {
    FilterDialog::Param p{"Gamma", 0.1, 5.0, 1.0, 490, false, ""};
    FilterDialog dlg("Gamma Correction", {p}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::gamma(img, v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Gamma", ImageProcessor::gamma(m_currentImage, dlg.values()[0]));
}

void MainWindow::onGrayscale() {
    applyOperation("Grayscale", ImageProcessor::toGrayscale(m_currentImage));
}

void MainWindow::onNegative() {
    applyOperation("Negative", ImageProcessor::negative(m_currentImage));
}

void MainWindow::onHistogramEq() {
    applyOperation("Histogram Eq.", ImageProcessor::histogramEqualization(m_currentImage));
}

// ── Filter slots ──────────────────────────────────────────────────────────────

void MainWindow::onGaussianBlur() {
    FilterDialog::Param p{"Sigma", 0.5, 10.0, 2.0, 190, false, ""};
    FilterDialog dlg("Gaussian Blur", {p}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::gaussianBlur(img, v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Gaussian Blur", ImageProcessor::gaussianBlur(m_currentImage, dlg.values()[0]));
}

void MainWindow::onMedianBlur() {
    FilterDialog::Param p{"Radius", 1, 10, 2, 9, true, ""};
    FilterDialog dlg("Median Blur", {p}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::medianBlur(img, (int)v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Median Blur", ImageProcessor::medianBlur(m_currentImage, (int)dlg.values()[0]));
}

void MainWindow::onSharpen() {
    FilterDialog::Param p{"Amount", 0.1, 5.0, 1.5, 490, false, ""};
    FilterDialog dlg("Sharpen", {p}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::sharpen(img, v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Sharpen", ImageProcessor::sharpen(m_currentImage, dlg.values()[0]));
}

void MainWindow::onSobel() {
    applyOperation("Sobel Edge", ImageProcessor::sobelEdge(m_currentImage));
}

void MainWindow::onCanny() {
    FilterDialog::Param p{"Sigma", 0.5, 5.0, 1.0, 90, false, ""};
    FilterDialog dlg("Canny Edge", {p}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::cannyEdge(img, v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Canny Edge", ImageProcessor::cannyEdge(m_currentImage, dlg.values()[0]));
}

void MainWindow::onEmboss() {
    applyOperation("Emboss", ImageProcessor::emboss(m_currentImage));
}

// ── Transform slots ───────────────────────────────────────────────────────────

void MainWindow::onRotateCW()  { applyOperation("Rotate CW",  ImageProcessor::rotate90CW(m_currentImage)); }
void MainWindow::onRotateCCW() { applyOperation("Rotate CCW", ImageProcessor::rotate90CCW(m_currentImage)); }
void MainWindow::onFlipH()     { applyOperation("Flip H",     ImageProcessor::flipH(m_currentImage)); }
void MainWindow::onFlipV()     { applyOperation("Flip V",     ImageProcessor::flipV(m_currentImage)); }

void MainWindow::onCrop() {
    if (!m_actCrop->isChecked()) m_actCrop->setChecked(true);
}

void MainWindow::performCrop(QRect r) {
    if (r.isNull() || r.isEmpty()) return;
    applyOperation("Crop", ImageProcessor::crop(m_currentImage,
                                                 r.left(), r.top(),
                                                 r.right(), r.bottom()));
}

void MainWindow::onResize() {
    if (m_currentImage.is_empty()) return;
    int origW = m_currentImage.width(), origH = m_currentImage.height();

    QDialog dlg(this);
    dlg.setWindowTitle("Resize Image");
    auto* form = new QFormLayout(&dlg);

    auto* wSpin = new QSpinBox(&dlg); wSpin->setRange(1, 16384); wSpin->setValue(origW);
    auto* hSpin = new QSpinBox(&dlg); hSpin->setRange(1, 16384); hSpin->setValue(origH);
    auto* lockAR = new QCheckBox("Keep aspect ratio", &dlg); lockAR->setChecked(true);

    form->addRow("Width:",  wSpin);
    form->addRow("Height:", hSpin);
    form->addRow(lockAR);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    form->addRow(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // Maintain aspect ratio
    connect(wSpin, QOverload<int>::of(&QSpinBox::valueChanged), [&](int w) {
        if (lockAR->isChecked()) {
            QSignalBlocker b(hSpin);
            hSpin->setValue(std::max(1, (int)std::round((double)w / origW * origH)));
        }
    });
    connect(hSpin, QOverload<int>::of(&QSpinBox::valueChanged), [&](int h) {
        if (lockAR->isChecked()) {
            QSignalBlocker b(wSpin);
            wSpin->setValue(std::max(1, (int)std::round((double)h / origH * origW)));
        }
    });

    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Resize", ImageProcessor::resize(m_currentImage, wSpin->value(), hSpin->value()));
}

// ── Morphology slots ──────────────────────────────────────────────────────────

static FilterDialog::Param morphRadiusParam() {
    return {"Radius", 1, 10, 2, 9, true, ""};
}

void MainWindow::onErode() {
    FilterDialog dlg("Erode", {morphRadiusParam()}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::erode(img, (int)v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Erode", ImageProcessor::erode(m_currentImage, (int)dlg.values()[0]));
}

void MainWindow::onDilate() {
    FilterDialog dlg("Dilate", {morphRadiusParam()}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::dilate(img, (int)v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Dilate", ImageProcessor::dilate(m_currentImage, (int)dlg.values()[0]));
}

void MainWindow::onMorphOpen() {
    FilterDialog dlg("Morphological Open", {morphRadiusParam()}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::morphOpen(img, (int)v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Morph. Open", ImageProcessor::morphOpen(m_currentImage, (int)dlg.values()[0]));
}

void MainWindow::onMorphClose() {
    FilterDialog dlg("Morphological Close", {morphRadiusParam()}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::morphClose(img, (int)v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Morph. Close", ImageProcessor::morphClose(m_currentImage, (int)dlg.values()[0]));
}

void MainWindow::onMorphGradient() {
    FilterDialog dlg("Morphological Gradient", {morphRadiusParam()}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::morphGradient(img, (int)v[0]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Morph. Gradient", ImageProcessor::morphGradient(m_currentImage, (int)dlg.values()[0]));
}

// ── File browser ──────────────────────────────────────────────────────────────

void MainWindow::onFileBrowserClicked(const QModelIndex& idx) {
    QString path = m_fsModel->filePath(idx);
    if (!m_fsModel->isDir(idx)) loadImage(path);
}

// ── Help ─────────────────────────────────────────────────────────────────────

void MainWindow::onKeyboardShortcuts() {
    QMessageBox::information(this, "Keyboard Shortcuts",
        "File\n"
        "  Ctrl+O          Open file\n"
        "  Ctrl+Shift+O    Open folder\n"
        "  Ctrl+S          Save\n"
        "  Ctrl+Shift+S    Save As\n"
        "  Ctrl+Q          Exit\n"
        "\n"
        "Edit\n"
        "  Ctrl+Z          Undo\n"
        "  Ctrl+Y          Redo\n"
        "\n"
        "View\n"
        "  Ctrl++          Zoom In\n"
        "  Ctrl+-          Zoom Out\n"
        "  F               Fit to Window (toggle)\n"
        "  Ctrl+0          Actual Size (100%)\n"
        "\n"
        "Transform\n"
        "  Ctrl+]          Rotate CW\n"
        "  Ctrl+[          Rotate CCW\n"
        "  C               Crop (draw rectangle)"
    );
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About CImg Viewer",
        "<b>CImg Viewer</b><br><br>"
        "A cross-platform image viewer and editor built with:<br>"
        "&bull; <b>CImg</b> (header-only image processing)<br>"
        "&bull; <b>Qt 6</b> (GUI framework)<br><br>"
        "Based on the book:<br>"
        "<i>Digital Image Processing with C++: Implementing Reference Algorithms "
        "with the CImg Library</i><br>"
        "Tschumperlé, Tilmant, Barra — CRC Press, 2023"
    );
}

// ── Drag & Drop ───────────────────────────────────────────────────────────────

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    const auto urls = e->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString path = urls.first().toLocalFile();
        if (!path.isEmpty()) loadImage(path);
    }
}

// ── Close / Settings ──────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent* e) {
    saveSettings();
    e->accept();
}

void MainWindow::saveSettings() {
    QSettings s("CImgViewer", "CImgViewer");
    s.setValue("geometry",   saveGeometry());
    s.setValue("windowState", saveState());
}

void MainWindow::loadSettings() {
    QSettings s("CImgViewer", "CImgViewer");
    if (s.contains("geometry"))    restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("windowState")) restoreState(s.value("windowState").toByteArray());
}

// ── Compare mode ──────────────────────────────────────────────────────────────

void MainWindow::onCompareMode(bool on) {
    if (on) {
        if (m_currentImage.is_empty()) { m_actCompare->setChecked(false); return; }
        // Populate both panels
        QPixmap orig = QPixmap::fromImage(ImageConvert::toQImage(m_originalImage));
        QPixmap edit = QPixmap::fromImage(ImageConvert::toQImage(m_currentImage));
        m_compareView->setOriginal(orig);
        m_compareView->setEdited(edit);
        m_compareView->setZoom(m_zoom);
        // Swap central widget
        takeCentralWidget();  // detach scroll area without deleting
        setCentralWidget(m_compareView);
        m_compareView->show();
    } else {
        takeCentralWidget();  // detach compare view
        setCentralWidget(m_scrollArea);
        m_scrollArea->show();
    }
}

// ── Folder navigation / slideshow ─────────────────────────────────────────────

void MainWindow::buildFolderList(const QString& dir) {
    static const QStringList filters{"*.jpg","*.jpeg","*.png","*.bmp",
                                     "*.tiff","*.tif","*.pbm","*.pgm","*.ppm"};
    QDir d(dir);
    QStringList entries = d.entryList(filters, QDir::Files, QDir::Name | QDir::IgnoreCase);
    m_folderImages.clear();
    for (const QString& e : entries)
        m_folderImages << d.absoluteFilePath(e);
}

int MainWindow::currentFolderIndex() const {
    if (m_currentFilePath.isEmpty()) return -1;
    return m_folderImages.indexOf(m_currentFilePath);
}

void MainWindow::navigateToIndex(int idx) {
    if (m_folderImages.isEmpty()) return;
    // Wrap around
    idx = ((idx % m_folderImages.size()) + m_folderImages.size()) % m_folderImages.size();
    loadImage(m_folderImages[idx]);
}

void MainWindow::onPrevImage() {
    int idx = currentFolderIndex();
    if (idx < 0) return;
    navigateToIndex(idx - 1);
}

void MainWindow::onNextImage() {
    int idx = currentFolderIndex();
    if (idx < 0) return;
    navigateToIndex(idx + 1);
}

void MainWindow::onSlideshow(bool on) {
    if (on) {
        if (m_folderImages.isEmpty()) {
            m_actSlideshow->setChecked(false);
            return;
        }
        int ms = m_slideshowCombo->currentData().toInt();
        m_slideshowTimer->start(ms);
        m_slideshowCombo->setVisible(true);
        m_actSlideshow->setIcon(QIcon::fromTheme("media-playback-stop"));
        m_actSlideshow->setToolTip("Stop Slideshow (F5)");
    } else {
        m_slideshowTimer->stop();
        m_slideshowCombo->setVisible(false);
        m_actSlideshow->setIcon(QIcon::fromTheme("media-playback-start"));
        m_actSlideshow->setToolTip("Start Slideshow (F5)");
    }
    updateStatusBar();
}

void MainWindow::onSlideshowTick() {
    onNextImage();
}

// ── Keyboard ─────────────────────────────────────────────────────────────────

void MainWindow::keyPressEvent(QKeyEvent* e) {
    // Escape cancels the crop tool or stops slideshow
    if (e->key() == Qt::Key_Escape) {
        if (m_actCrop->isChecked()) {
            m_actCrop->setChecked(false);
            m_imageLabel->setTool(ImageLabel::Tool::None);
            return;
        }
        if (m_actSlideshow->isChecked()) {
            m_actSlideshow->setChecked(false);
            return;
        }
    }
    QMainWindow::keyPressEvent(e);
}
