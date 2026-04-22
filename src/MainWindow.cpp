#include "MainWindow.hpp"
#include "ImageProcessor.hpp"
#include "FileManager.hpp"
#include "FilterDialog.hpp"
#include "CompareView.hpp"
#include "BatchExportDialog.hpp"
#include "ExifPanel.hpp"
#include "SegmentTool.hpp"
#include "AnnotationLayer.hpp"
#include "AiTools.hpp"
#include "OnnxEngine.hpp"
#include "ModelManager.hpp"
#include "ModelManagerDialog.hpp"
#include "Logger.hpp"
#include <QMouseEvent>
#include <QColorDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QJsonDocument>
#include <QFileInfo>

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QFrame>
#include <QListWidget>
#include <QStandardPaths>
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
    setWindowTitle(tr("CImg Viewer"));
    setMinimumSize(900, 650);
    setAcceptDrops(true);

    m_annotLayer = std::make_unique<AnnotationLayer>();
    m_annotLayer->clear();

    OnnxEngine::instance().initialize();

    // Central widget: scroll area + image label
    m_imageLabel = new ImageLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_imageLabel->setBackgroundRole(QPalette::Dark);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setBackgroundRole(QPalette::Dark);

    // Compare view — hidden until compare mode is activated
    m_compareView = new CompareView(this);
    m_compareView->hide();

    // Use the scroll area as the default central widget
    setCentralWidget(m_scrollArea);

    createFileBrowserDock();
    createHistogramDock();
    createExifDock();
    createActions();
    createMenus();
    createToolBar();
    createAnnotationToolBar();
    createStatusBarWidgets();

    m_scrollArea->viewport()->installEventFilter(this);
    m_scrollArea->viewport()->setCursor(Qt::OpenHandCursor);
    connect(m_imageLabel, &ImageLabel::cropSelected,    this, &MainWindow::performCrop);
    connect(m_imageLabel, &ImageLabel::pixelClicked,    this, &MainWindow::onMagicSelectClicked);
    connect(m_imageLabel, &ImageLabel::annotatePress,   this, &MainWindow::onAnnotatePress);
    connect(m_imageLabel, &ImageLabel::annotateMove,    this, &MainWindow::onAnnotateMove);
    connect(m_imageLabel, &ImageLabel::annotateRelease, this, &MainWindow::onAnnotateRelease);

    loadSettings();
    updateActions();
}

MainWindow::~MainWindow() {}

// ── Dock: file browser ───────────────────────────────────────────────────────

void MainWindow::createFileBrowserDock() {
    m_fileDock = new QDockWidget(tr("File Browser"), this);
    m_fileDock->setObjectName("fileDock");

    auto* container = new QWidget(this);
    auto* vLayout   = new QVBoxLayout(container);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    // ── Quick Access panel ─────────────────────────────────────────────────────
    auto* favHeader = new QLabel(tr("  Quick Access"), container);
    favHeader->setStyleSheet(
        "background: palette(mid); padding: 3px 4px; font-weight: bold; font-size: 11px;");
    vLayout->addWidget(favHeader);

    m_favList = new QListWidget(container);
    m_favList->setFrameShape(QFrame::NoFrame);
    m_favList->setIconSize(QSize(16, 16));
    m_favList->setMaximumHeight(170);
    m_favList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_favList->setContextMenuPolicy(Qt::CustomContextMenu);
    populateFavorites();
    connect(m_favList, &QListWidget::itemClicked,
            this, &MainWindow::onFavItemClicked);
    connect(m_favList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onFavContextMenu);
    vLayout->addWidget(m_favList);

    // ── File system tree ───────────────────────────────────────────────────────
    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setNameFilters(FileManager::supportedExtensions());
    m_fsModel->setNameFilterDisables(false);

#ifdef Q_OS_WIN
    // On Windows, root path "" exposes all drives (My Computer level)
    m_fsModel->setRootPath("");
    m_fileTree = new QTreeView(container);
    m_fileTree->setModel(m_fsModel);
    m_fileTree->setRootIndex(QModelIndex()); // invalid = computer root
#else
    m_fsModel->setRootPath("/");
    m_fileTree = new QTreeView(container);
    m_fileTree->setModel(m_fsModel);
    m_fileTree->setRootIndex(m_fsModel->index("/"));
#endif

    m_fileTree->setColumnHidden(1, true);
    m_fileTree->setColumnHidden(2, true);
    m_fileTree->setColumnHidden(3, true);
    m_fileTree->header()->hide();
    m_fileTree->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_fileTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_fileTree, &QTreeView::clicked,
            this, &MainWindow::onFileBrowserClicked);
    connect(m_fileTree, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);
    vLayout->addWidget(m_fileTree);

    m_fileDock->setWidget(container);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileDock);
}

void MainWindow::populateFavorites() {
    m_favList->clear();

    // Standard locations
    struct Loc { QString label; QStandardPaths::StandardLocation id; };
    static const Loc locs[] = {
        { tr("Home"),      QStandardPaths::HomeLocation      },
        { tr("Desktop"),   QStandardPaths::DesktopLocation   },
        { tr("Documents"), QStandardPaths::DocumentsLocation },
        { tr("Downloads"), QStandardPaths::DownloadLocation  },
        { tr("Pictures"),  QStandardPaths::PicturesLocation  },
    };
    for (const auto& loc : locs) {
        QString path = QStandardPaths::writableLocation(loc.id);
        if (path.isEmpty() || !QDir(path).exists()) continue;
        auto* item = new QListWidgetItem(QIcon::fromTheme("folder"), loc.label);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_favList->addItem(item);
    }

#ifdef Q_OS_WIN
    // List all available drives
    auto* driveSep = new QListWidgetItem(tr("─── Drives ───"));
    driveSep->setFlags(Qt::NoItemFlags);
    driveSep->setForeground(QApplication::palette().color(QPalette::PlaceholderText));
    m_favList->addItem(driveSep);
    for (const QFileInfo& drive : QDir::drives()) {
        QString d = drive.absoluteFilePath();
        auto* item = new QListWidgetItem(QIcon::fromTheme("drive-harddisk"), d);
        item->setData(Qt::UserRole, d);
        item->setToolTip(d);
        m_favList->addItem(item);
    }
#endif

    // User custom favorites from QSettings
    QSettings s("CImgViewer", "CImgViewer");
    QStringList custom = s.value("customFavorites").toStringList();
    custom.erase(std::remove_if(custom.begin(), custom.end(),
        [](const QString& p){ return !QDir(p).exists(); }), custom.end());
    if (!custom.isEmpty()) {
        auto* custSep = new QListWidgetItem(tr("─── Custom ───"));
        custSep->setFlags(Qt::NoItemFlags);
        custSep->setForeground(QApplication::palette().color(QPalette::PlaceholderText));
        m_favList->addItem(custSep);
        for (const QString& path : custom) {
            auto* item = new QListWidgetItem(
                QIcon::fromTheme("folder-bookmark"), QFileInfo(path).fileName());
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            m_favList->addItem(item);
        }
    }
}

void MainWindow::navigateFileBrowserTo(const QString& dir) {
    QModelIndex idx = m_fsModel->index(dir);
    if (idx.isValid()) {
        m_fileTree->setCurrentIndex(idx);
        m_fileTree->scrollTo(idx, QAbstractItemView::PositionAtTop);
        m_fileTree->expand(idx);
    }
}

void MainWindow::onFavItemClicked(QListWidgetItem* item) {
    QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    navigateFileBrowserTo(path);
    buildFolderList(path);
}

void MainWindow::onFavContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_favList->itemAt(pos);
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;
    QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;

    // Only allow removing custom favorites (not standard locations or drives)
    QSettings s("CImgViewer", "CImgViewer");
    QStringList custom = s.value("customFavorites").toStringList();
    if (!custom.contains(path)) return;

    QMenu menu(this);
    QAction* actRemove = menu.addAction(QIcon::fromTheme("list-remove"),
                                        tr("Remove from Favorites"));
    if (menu.exec(m_favList->mapToGlobal(pos)) == actRemove) {
        custom.removeAll(path);
        s.setValue("customFavorites", custom);
        populateFavorites();
    }
}

void MainWindow::onTreeContextMenu(const QPoint& pos) {
    QModelIndex idx = m_fileTree->indexAt(pos);
    if (!idx.isValid()) return;
    if (!m_fsModel->isDir(idx)) return;

    QString path = m_fsModel->filePath(idx);
    QMenu menu(this);
    QAction* actAdd = menu.addAction(QIcon::fromTheme("list-add"),
                                     tr("Add to Favorites"));
    if (menu.exec(m_fileTree->viewport()->mapToGlobal(pos)) == actAdd) {
        QSettings s("CImgViewer", "CImgViewer");
        QStringList custom = s.value("customFavorites").toStringList();
        if (!custom.contains(path)) {
            custom.append(path);
            s.setValue("customFavorites", custom);
            populateFavorites();
        }
    }
}

// ── Dock: histogram ──────────────────────────────────────────────────────────

void MainWindow::createHistogramDock() {
    m_histDock = new QDockWidget(tr("Histogram"), this);
    m_histDock->setObjectName("histDock");

    m_histWidget = new HistogramWidget(this);
    m_histDock->setWidget(m_histWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_histDock);
}

// ── Dock: EXIF panel ─────────────────────────────────────────────────────────

void MainWindow::createExifDock() {
    m_exifDock = new QDockWidget(tr("EXIF Info"), this);
    m_exifDock->setObjectName("exifDock");
    m_exifPanel = new ExifPanel(this);
    m_exifDock->setWidget(m_exifPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_exifDock);
    m_exifDock->hide(); // hidden by default
}

// ── Actions ──────────────────────────────────────────────────────────────────

void MainWindow::createActions() {
    // File
    m_actOpen = new QAction(QIcon::fromTheme("document-open"), tr("&Open…"), this);
    m_actOpen->setShortcut(QKeySequence::Open);
    connect(m_actOpen, &QAction::triggered, this, &MainWindow::onOpenFile);

    m_actOpenFolder = new QAction(QIcon::fromTheme("folder-open"), tr("Open &Folder…"), this);
    m_actOpenFolder->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_O);
    connect(m_actOpenFolder, &QAction::triggered, this, &MainWindow::onOpenFolder);

    m_actSave = new QAction(QIcon::fromTheme("document-save"), tr("&Save"), this);
    m_actSave->setShortcut(QKeySequence::Save);
    connect(m_actSave, &QAction::triggered, this, &MainWindow::onSave);

    m_actSaveAs = new QAction(QIcon::fromTheme("document-save-as"), tr("Save &As…"), this);
    m_actSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(m_actSaveAs, &QAction::triggered, this, &MainWindow::onSaveAs);

    // Edit
    m_actUndo = new QAction(QIcon::fromTheme("edit-undo"), tr("&Undo"), this);
    m_actUndo->setShortcut(QKeySequence::Undo);
    connect(m_actUndo, &QAction::triggered, this, &MainWindow::onUndo);

    m_actRedo = new QAction(QIcon::fromTheme("edit-redo"), tr("&Redo"), this);
    m_actRedo->setShortcut(QKeySequence::Redo);
    connect(m_actRedo, &QAction::triggered, this, &MainWindow::onRedo);

    // View
    m_actZoomIn = new QAction(QIcon::fromTheme("zoom-in"), tr("Zoom &In"), this);
    m_actZoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(m_actZoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);

    m_actZoomOut = new QAction(QIcon::fromTheme("zoom-out"), tr("Zoom &Out"), this);
    m_actZoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(m_actZoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);

    m_actFitWindow = new QAction(QIcon::fromTheme("zoom-fit-best"), tr("&Fit to Window"), this);
    m_actFitWindow->setShortcut(Qt::Key_F);
    m_actFitWindow->setCheckable(true);
    connect(m_actFitWindow, &QAction::toggled, this, &MainWindow::onFitToWindow);

    m_actActualSize = new QAction(QIcon::fromTheme("zoom-original"), tr("Actual &Size (100%)"), this);
    m_actActualSize->setShortcut(Qt::CTRL | Qt::Key_0);
    connect(m_actActualSize, &QAction::triggered, this, &MainWindow::onActualSize);

    // Folder navigation
    m_actPrev = new QAction(QIcon::fromTheme("go-previous"), tr("&Previous Image"), this);
    m_actPrev->setShortcut(Qt::Key_Left);
    connect(m_actPrev, &QAction::triggered, this, &MainWindow::onPrevImage);
    m_imageActions.append(m_actPrev);

    m_actNext = new QAction(QIcon::fromTheme("go-next"), tr("&Next Image"), this);
    m_actNext->setShortcut(Qt::Key_Right);
    connect(m_actNext, &QAction::triggered, this, &MainWindow::onNextImage);
    m_imageActions.append(m_actNext);

    // Slideshow
    m_actSlideshow = new QAction(QIcon::fromTheme("media-playback-start"), tr("S&lideshow"), this);
    m_actSlideshow->setShortcut(Qt::Key_F5);
    m_actSlideshow->setCheckable(true);
    connect(m_actSlideshow, &QAction::toggled, this, &MainWindow::onSlideshow);
    m_imageActions.append(m_actSlideshow);

    m_slideshowTimer = new QTimer(this);
    connect(m_slideshowTimer, &QTimer::timeout, this, &MainWindow::onSlideshowTick);

    // Compare mode
    m_actCompare = new QAction(tr("&Compare Mode"), this);
    m_actCompare->setShortcut(Qt::Key_V);
    m_actCompare->setCheckable(true);
    m_actCompare->setToolTip(tr("Side-by-side comparison (V)"));
    connect(m_actCompare, &QAction::toggled, this, &MainWindow::onCompareMode);
    m_imageActions.append(m_actCompare);

    // Crop (toggle tool)
    m_actCrop = new QAction(tr("&Crop"), this);
    m_actCrop->setShortcut(Qt::Key_C);
    m_actCrop->setCheckable(true);
    connect(m_actCrop, &QAction::toggled, this, [this](bool on) {
        m_imageLabel->setTool(on ? ImageLabel::Tool::Crop : ImageLabel::Tool::None);
        if (!on) m_actCrop->setChecked(false);
    });
    connect(m_imageLabel, &ImageLabel::cropSelected, this, [this](QRect) {
        m_actCrop->setChecked(false);
    });

    // Magic Select
    m_actMagicSelect = new QAction(tr("&Magic Select"), this);
    m_actMagicSelect->setShortcut(Qt::Key_S);
    m_actMagicSelect->setCheckable(true);
    m_actMagicSelect->setToolTip(tr("Region-grow selection (S)"));
    connect(m_actMagicSelect, &QAction::toggled, this, &MainWindow::onMagicSelectToggled);
    m_imageActions.append(m_actMagicSelect);

    // Export Selection (enabled only when selection is active)
    m_actExportSelection = new QAction(tr("&Export Selection…"), this);
    m_actExportSelection->setEnabled(false);
    connect(m_actExportSelection, &QAction::triggered, this, &MainWindow::onExportSelection);

    // Annotate toggle
    m_actAnnotate = new QAction(tr("&Annotate"), this);
    m_actAnnotate->setShortcut(Qt::Key_A);
    m_actAnnotate->setCheckable(true);
    m_actAnnotate->setToolTip(tr("Toggle annotation layer (A)"));
    connect(m_actAnnotate, &QAction::toggled, this, &MainWindow::onAnnotateToggled);
    m_imageActions.append(m_actAnnotate);

    // Annotation file actions
    m_actSaveWithAnnot    = new QAction(tr("Save &with Annotations"),    this);
    m_actSaveWithoutAnnot = new QAction(tr("Save wit&hout Annotations"), this);
    m_actSaveAnnotJson    = new QAction(tr("Save Annotations as &JSON…"),this);
    connect(m_actSaveWithAnnot,    &QAction::triggered, this, &MainWindow::onSaveWithAnnotations);
    connect(m_actSaveWithoutAnnot, &QAction::triggered, this, &MainWindow::onSaveWithoutAnnotations);
    connect(m_actSaveAnnotJson,    &QAction::triggered, this, &MainWindow::onSaveAnnotationsJson);
    m_imageActions.append(m_actSaveWithAnnot);
    m_imageActions.append(m_actSaveWithoutAnnot);
    m_imageActions.append(m_actSaveAnnotJson);
}

// ── Menus ────────────────────────────────────────────────────────────────────

void MainWindow::createMenus() {
    // ── File ──────────────────────────────────────────────────────────────────
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actOpen);
    fileMenu->addAction(m_actOpenFolder);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSave);
    fileMenu->addAction(m_actSaveAs);
    fileMenu->addSeparator();

    m_recentMenu = fileMenu->addMenu(tr("&Recent Files"));
    rebuildRecentMenu();

    fileMenu->addSeparator();
    fileMenu->addAction(m_actSaveWithAnnot);
    fileMenu->addAction(m_actSaveWithoutAnnot);
    fileMenu->addAction(m_actSaveAnnotJson);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Batch Export…"), this, [this]() {
        BatchExportDialog dlg(this);
        dlg.exec();
    });
    fileMenu->addSeparator();
    QAction* actExit = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    actExit->setShortcut(QKeySequence::Quit);

    // ── Edit ──────────────────────────────────────────────────────────────────
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_actUndo);
    editMenu->addAction(m_actRedo);
    editMenu->addSeparator();
    editMenu->addAction(m_actExportSelection);

    // ── View ──────────────────────────────────────────────────────────────────
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
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
    QAction* exifAct = m_exifDock->toggleViewAction();
    exifAct->setText(tr("&EXIF Info"));
    exifAct->setShortcut(Qt::Key_E);
    viewMenu->addAction(exifAct);

    // ── Image ─────────────────────────────────────────────────────────────────
    QMenu* imgMenu = menuBar()->addMenu(tr("&Image"));

    // Adjustments
    QMenu* adjMenu = imgMenu->addMenu(tr("&Adjustments"));
    auto addAdj = [&](const QString& name, auto slot) {
        QAction* a = adjMenu->addAction(name, this, slot);
        m_imageActions.append(a);
        return a;
    };
    addAdj(tr("&Brightness…"),           &MainWindow::onBrightness);
    addAdj(tr("&Contrast…"),             &MainWindow::onContrast);
    addAdj(tr("&Gamma Correction…"),     &MainWindow::onGamma);
    adjMenu->addSeparator();
    addAdj(tr("Convert to &Grayscale"),  &MainWindow::onGrayscale);
    addAdj(tr("&Colorize (LAB Transfer)…"), &MainWindow::onColorizeLabTransfer);
    addAdj(tr("&Negative (Invert)"),     &MainWindow::onNegative);
    addAdj(tr("&Histogram Equalization"),&MainWindow::onHistogramEq);

    // Filters
    QMenu* filtMenu = imgMenu->addMenu(tr("&Filters"));
    auto addFilt = [&](const QString& name, auto slot) {
        QAction* a = filtMenu->addAction(name, this, slot);
        m_imageActions.append(a);
        return a;
    };
    addFilt(tr("&Gaussian Blur…"),  &MainWindow::onGaussianBlur);
    addFilt(tr("&Median Blur…"),    &MainWindow::onMedianBlur);
    addFilt(tr("&Sharpen…"),        &MainWindow::onSharpen);
    filtMenu->addSeparator();
    addFilt(tr("&Sobel Edge"),      &MainWindow::onSobel);
    addFilt(tr("&Canny Edge…"),     &MainWindow::onCanny);
    addFilt(tr("&Emboss"),          &MainWindow::onEmboss);
    filtMenu->addSeparator();
    addFilt(tr("&Non-linear Diffusion…"), &MainWindow::onNonLinearDiffusion);

    // Transform
    QMenu* trMenu = imgMenu->addMenu(tr("&Transform"));
    auto addTr = [&](const QString& name, auto slot, QKeySequence sc = {}) {
        QAction* a = trMenu->addAction(name, this, slot);
        if (!sc.isEmpty()) a->setShortcut(sc);
        m_imageActions.append(a);
        return a;
    };
    addTr(tr("Rotate &Right (CW)"),      &MainWindow::onRotateCW,  Qt::CTRL | Qt::Key_BracketRight);
    addTr(tr("Rotate &Left (CCW)"),      &MainWindow::onRotateCCW, Qt::CTRL | Qt::Key_BracketLeft);
    addTr(tr("Flip &Horizontal"),        &MainWindow::onFlipH);
    addTr(tr("Flip &Vertical"),          &MainWindow::onFlipV);
    trMenu->addSeparator();
    m_imageActions.append(m_actCrop);
    trMenu->addAction(m_actCrop);
    addTr(tr("&Resize…"),                &MainWindow::onResize);

    // Morphology
    QMenu* morphMenu = imgMenu->addMenu(tr("&Morphology"));
    auto addMorph = [&](const QString& name, auto slot) {
        QAction* a = morphMenu->addAction(name, this, slot);
        m_imageActions.append(a);
        return a;
    };
    addMorph(tr("&Erode…"),            &MainWindow::onErode);
    addMorph(tr("&Dilate…"),           &MainWindow::onDilate);
    morphMenu->addSeparator();
    addMorph(tr("M. &Open…"),          &MainWindow::onMorphOpen);
    addMorph(tr("M. &Close…"),         &MainWindow::onMorphClose);
    addMorph(tr("M. &Gradient…"),      &MainWindow::onMorphGradient);

    // ── AI ────────────────────────────────────────────────────────────────────
    QMenu* aiMenu = menuBar()->addMenu(tr("A&I"));
    auto addAi = [&](const QString& name, auto slot) {
        QAction* a = aiMenu->addAction(name, this, slot);
        m_imageActions.append(a);
        return a;
    };
    // "Remove Background" disabled — model not bundled in this build
    // addAi(tr("&Remove Background"),   &MainWindow::onAiRemoveBg);
    addAi(tr("&Super Resolution ×4"), &MainWindow::onAiSuperRes);
    addAi(tr("&Detect Objects"),      &MainWindow::onAiDetect);

    // Style Transfer sub-menu (4 styles)
    QMenu* styleMenu = aiMenu->addMenu(tr("&Style Transfer"));
    auto addStyle = [&](const QString& label, const QString& styleId) {
        QAction* a = styleMenu->addAction(label, this, [this, styleId]() {
            onAiStyleTransfer(styleId);
        });
        m_imageActions.append(a);
    };
    addStyle(tr("&Candy"),          "style_candy");
    addStyle(tr("&Mosaic"),         "style_mosaic");
    addStyle(tr("&Pointilism"),     "style_pointilism");
    addStyle(tr("Rain &Princess"),  "style_rain_princess");

    aiMenu->addSeparator();
    aiMenu->addAction(tr("&Manage Models…"), this, &MainWindow::onAiManageModels);

    // ── Settings ──────────────────────────────────────────────────────────────
    QMenu* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    QMenu* langMenu = settingsMenu->addMenu(tr("&Language"));
    QAction* actLangPtBr = langMenu->addAction(tr("Português (Brasil)"),
        this, &MainWindow::onLanguagePtBr);
    actLangPtBr->setObjectName("actLangPtBr");
    QAction* actLangEn = langMenu->addAction(tr("English"),
        this, &MainWindow::onLanguageEn);
    actLangEn->setObjectName("actLangEn");

    // ── Tools ─────────────────────────────────────────────────────────────────
    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(m_actMagicSelect);
    toolsMenu->addAction(m_actAnnotate);

    // ── Help ──────────────────────────────────────────────────────────────────
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Keyboard Shortcuts"), this, &MainWindow::onKeyboardShortcuts);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("Open &Log File"),   this, &MainWindow::onOpenLogFile);
    helpMenu->addAction(tr("Open Log &Folder"), this, &MainWindow::onOpenLogFolder);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About"), this, &MainWindow::onAbout);
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
    addTbBtn("↻",  &MainWindow::onRotateCW)->setToolTip(tr("Rotate CW"));
    addTbBtn("↺",  &MainWindow::onRotateCCW)->setToolTip(tr("Rotate CCW"));
    addTbBtn("↔",  &MainWindow::onFlipH)->setToolTip(tr("Flip Horizontal"));
    addTbBtn("↕",  &MainWindow::onFlipV)->setToolTip(tr("Flip Vertical"));

    tb->addAction(m_actCompare);
    tb->addSeparator();
    tb->addAction(m_actPrev);
    tb->addAction(m_actNext);
    tb->addAction(m_actSlideshow);

    // Magic Select threshold widget — visible only when MagicSelect is active
    {
        auto* w = new QWidget(tb);
        auto* lay = new QHBoxLayout(w);
        lay->setContentsMargins(4, 0, 4, 0);
        m_thresholdLabel  = new QLabel(tr("Threshold: 30"), w);
        m_thresholdSlider = new QSlider(Qt::Horizontal, w);
        m_thresholdSlider->setRange(0, 100);
        m_thresholdSlider->setValue(m_selectThreshold);
        m_thresholdSlider->setFixedWidth(120);
        m_thresholdSlider->setToolTip(tr("Region growing colour tolerance"));
        lay->addWidget(m_thresholdLabel);
        lay->addWidget(m_thresholdSlider);
        m_thresholdAction = tb->addWidget(w);
        m_thresholdAction->setVisible(false);
        connect(m_thresholdSlider, &QSlider::valueChanged,
                this, &MainWindow::onThresholdChanged);
    }

    // Interval combo — visible only during slideshow
    m_slideshowCombo = new QComboBox(tb);
    m_slideshowCombo->addItem("2 s",  2000);
    m_slideshowCombo->addItem("5 s",  5000);
    m_slideshowCombo->addItem("10 s", 10000);
    m_slideshowCombo->addItem("30 s", 30000);
    m_slideshowCombo->setCurrentIndex(1); // 5 s default
    m_slideshowCombo->setVisible(false);
    m_slideshowCombo->setToolTip(tr("Slideshow interval"));
    connect(m_slideshowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (m_slideshowTimer->isActive()) {
                    int ms = m_slideshowCombo->currentData().toInt();
                    m_slideshowTimer->setInterval(ms);
                }
            });
    m_slideshowComboAction = tb->addWidget(m_slideshowCombo);
}

// ── Annotation toolbar ────────────────────────────────────────────────────────

void MainWindow::createAnnotationToolBar() {
    m_annotToolBar = addToolBar("Annotations");
    m_annotToolBar->setObjectName("annotToolBar");
    m_annotToolBar->setMovable(false);
    m_annotToolBar->setVisible(false);  // shown only when annotation mode is active

    // Annotation tool buttons (exclusive)
    m_annotToolGroup = new QActionGroup(this);
    m_annotToolGroup->setExclusive(true);

    auto addTool = [&](const QString& label, AnnotationLayer::DrawTool t, const QString& tip) {
        QAction* a = m_annotToolBar->addAction(label);
        a->setCheckable(true);
        a->setToolTip(tip);
        m_annotToolGroup->addAction(a);
        connect(a, &QAction::toggled, this, [this, t](bool on) {
            if (on && m_annotLayer) m_annotLayer->setDrawTool(t);
        });
        return a;
    };

    QAction* aArrow = addTool("→ Arrow",   AnnotationLayer::DrawTool::Arrow,    tr("Arrow (line + arrowhead)"));
    addTool("▭ Rect",    AnnotationLayer::DrawTool::Rectangle, tr("Rectangle"));
    addTool("⬭ Ellipse", AnnotationLayer::DrawTool::Ellipse,   tr("Ellipse"));
    addTool("T Text",    AnnotationLayer::DrawTool::Text,      tr("Text"));
    addTool("✏ Pen",     AnnotationLayer::DrawTool::Freehand,  tr("Freehand pen"));
    addTool("⌫ Eraser",  AnnotationLayer::DrawTool::Eraser,    tr("Eraser"));
    aArrow->setChecked(true);

    m_annotToolBar->addSeparator();

    // Color button
    m_actAnnotColor = m_annotToolBar->addAction(tr("● Color"));
    m_actAnnotColor->setToolTip(tr("Choose annotation colour"));
    connect(m_actAnnotColor, &QAction::triggered, this, &MainWindow::onAnnotateColor);

    // Line width
    m_annotToolBar->addWidget(new QLabel(" Width:"));
    m_annotLineWidthSpin = new QSpinBox(m_annotToolBar);
    m_annotLineWidthSpin->setRange(1, 20);
    m_annotLineWidthSpin->setValue(2);
    m_annotLineWidthSpin->setToolTip(tr("Line width (px)"));
    m_annotToolBar->addWidget(m_annotLineWidthSpin);
    connect(m_annotLineWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) { if (m_annotLayer) m_annotLayer->setLineWidth(v); });

    // Font size
    m_annotToolBar->addWidget(new QLabel(" Font:"));
    m_annotFontSizeSpin = new QSpinBox(m_annotToolBar);
    m_annotFontSizeSpin->setRange(6, 72);
    m_annotFontSizeSpin->setValue(14);
    m_annotFontSizeSpin->setToolTip(tr("Font size for text annotations"));
    m_annotToolBar->addWidget(m_annotFontSizeSpin);
    connect(m_annotFontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) { if (m_annotLayer) m_annotLayer->setFontSize(v); });

    m_annotToolBar->addSeparator();

    // Undo/redo for annotations
    m_actAnnotUndo = m_annotToolBar->addAction(QIcon::fromTheme("edit-undo"), tr("Undo Annotation"));
    m_actAnnotRedo = m_annotToolBar->addAction(QIcon::fromTheme("edit-redo"), tr("Redo Annotation"));
    m_actAnnotUndo->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_Z);
    m_actAnnotRedo->setShortcut(Qt::CTRL | Qt::ALT | Qt::Key_Y);
    connect(m_actAnnotUndo, &QAction::triggered, this, &MainWindow::onAnnotateUndo);
    connect(m_actAnnotRedo, &QAction::triggered, this, &MainWindow::onAnnotateRedo);
}

// ── Status bar ───────────────────────────────────────────────────────────────

void MainWindow::createStatusBarWidgets() {
    m_statusImgInfo  = new QLabel(this);
    m_statusZoom     = new QLabel(this);
    m_statusPath     = new QLabel(this);
    m_statusSelection = new QLabel(this);
    m_statusPath->setTextFormat(Qt::PlainText);

    statusBar()->addWidget(m_statusImgInfo);
    statusBar()->addWidget(m_statusZoom);
    statusBar()->addWidget(m_statusPath, 1);
    statusBar()->addWidget(m_statusSelection);

    // AI provider indicator (permanent, right side)
    auto* aiLabel = new QLabel("  " + tr("AI") + ": " + OnnxEngine::instance().providerName() + "  ", this);
    aiLabel->setToolTip(tr("Active ONNX Runtime execution provider"));
    statusBar()->addPermanentWidget(aiLabel);
}

// ── Load / Display ───────────────────────────────────────────────────────────

void MainWindow::openPath(const QString& path) { loadImage(path); }

void MainWindow::loadImage(const QString& path) {
    LOG_INFO("MainWindow", QString("Opening: %1").arg(path));
    QString err;
    CImgU8 img = FileManager::loadImage(path, &err);
    if (img.is_empty()) {
        LOG_ERROR("MainWindow", QString("Failed to load '%1': %2").arg(path, err));
        QMessageBox::warning(this, tr("Open Error"), err);
        return;
    }
    LOG_INFO("MainWindow",
        QString("Loaded: %1 (%2×%3, %4 ch)")
            .arg(path).arg(img.width()).arg(img.height()).arg(img.spectrum()));
    m_currentImage     = img;
    m_originalImage    = img;   // freeze for compare view
    m_currentFilePath  = path;

    m_undoStack.clear();
    m_undoStack.push("Load", m_currentImage);

    clearSelection();
    if (m_actMagicSelect->isChecked())
        m_actMagicSelect->setChecked(false);

    // Reset annotation layer
    m_annotLayer->clear();
    loadAnnotationsForImage(path);
    updateAnnotationOverlay();
    updateAnnotationActions();

    // Reset zoom when loading a new image
    m_fitToWindow = false;
    m_actFitWindow->setChecked(false);
    m_zoom = 1.0;

    displayImage();
    updateHistogram();
    updateActions();
    updateStatusBar();
    m_exifPanel->loadFile(path);

    FileManager::addRecentFile(path);
    rebuildRecentMenu();

    setWindowTitle(QFileInfo(path).fileName() + " — " + tr("CImg Viewer"));
    buildFolderList(QFileInfo(path).absolutePath());
}

void MainWindow::displayImage() {
    if (m_currentImage.is_empty()) {
        m_imageLabel->clear();
        return;
    }

    QImage qi = ImageConvert::toQImage(m_currentImage);
    // Composite RGBA images over a checkerboard so transparency is visible
    if (qi.hasAlphaChannel())
        qi = ImageConvert::compositeOnCheckerboard(qi);
    QPixmap pm = QPixmap::fromImage(qi);

    if (m_fitToWindow) {
        QSize vp = m_scrollArea->viewport()->size();
        QPixmap scaled = pm.scaled(vp, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_zoom = (double)scaled.width() / pm.width();
        m_imageLabel->setDisplayZoom(m_zoom);
        m_imageLabel->setPixmap(scaled);
        m_imageLabel->setFixedSize(scaled.size());
    } else {
        if (m_zoom != 1.0) {
            pm = pm.scaled((int)std::round(pm.width()  * m_zoom),
                           (int)std::round(pm.height() * m_zoom),
                           Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
        }
        m_imageLabel->setDisplayZoom(m_zoom);
        m_imageLabel->setPixmap(pm);
        m_imageLabel->setFixedSize(pm.size());
    }
    updateStatusBar();
}

void MainWindow::applyOperation(const QString& name, const CImgU8& result) {
    m_currentImage = result;
    m_undoStack.push(name.toStdString(), m_currentImage);
    clearSelection();
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
            tr("Slideshow — %1/%2  %3")
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
    m_actUndo->setText(uname.isEmpty() ? tr("&Undo") : tr("&Undo ") + uname);
    m_actRedo->setText(rname.isEmpty() ? tr("&Redo") : tr("&Redo ") + rname);
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
    QString path = QFileDialog::getOpenFileName(this, tr("Open Image"), QString(),
                                                FileManager::imageFilter());
    if (!path.isEmpty()) loadImage(path);
}

void MainWindow::onOpenFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"),
                                                    QDir::homePath());
    if (dir.isEmpty()) return;
    navigateFileBrowserTo(dir);
    buildFolderList(dir);
    m_fileDock->show();
    m_fileDock->raise();
}

void MainWindow::onSave() {
    if (m_currentFilePath.isEmpty()) { onSaveAs(); return; }
    QString err;
    if (!FileManager::saveImage(m_currentImage, m_currentFilePath, &err)) {
        LOG_ERROR("MainWindow", QString("Save failed '%1': %2").arg(m_currentFilePath, err));
        QMessageBox::warning(this, tr("Save Error"), err);
    } else {
        LOG_INFO("MainWindow", QString("Saved: %1").arg(m_currentFilePath));
    }
}

void MainWindow::onSaveAs() {
    QString path = QFileDialog::getSaveFileName(this, tr("Save Image As"),
        m_currentFilePath.isEmpty() ? QDir::homePath() : m_currentFilePath,
        FileManager::saveFilter());
    if (path.isEmpty()) return;
    QString err;
    if (FileManager::saveImage(m_currentImage, path, &err)) {
        m_currentFilePath = path;
        LOG_INFO("MainWindow", QString("Saved as: %1").arg(path));
        setWindowTitle(QFileInfo(path).fileName() + " — " + tr("CImg Viewer"));
        updateStatusBar();
        FileManager::addRecentFile(path);
        rebuildRecentMenu();
    } else {
        LOG_ERROR("MainWindow", QString("Save-as failed '%1': %2").arg(path, err));
        QMessageBox::warning(this, tr("Save Error"), err);
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
        m_recentMenu->addAction(tr("&Clear Recent"), this, &MainWindow::onClearRecent);
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

void MainWindow::onNonLinearDiffusion() {
    FilterDialog::Param p1{"Iterations", 1, 50, 10, 49, true,  ""};
    FilterDialog::Param p2{"Kappa",      1, 100, 20, 99, false, ""};
    FilterDialog dlg("Non-linear Diffusion (Perona-Malik)", {p1, p2}, m_currentImage,
        [](const CImgU8& img, const QVector<double>& v) {
            return ImageProcessor::peronaMalikDiffusion(img, (int)v[0], v[1]);
        }, this);
    if (dlg.exec() == QDialog::Accepted)
        applyOperation("Non-linear Diffusion",
            ImageProcessor::peronaMalikDiffusion(
                m_currentImage, (int)dlg.values()[0], dlg.values()[1]));
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
    dlg.setWindowTitle(tr("Resize Image"));
    auto* form = new QFormLayout(&dlg);

    auto* wSpin = new QSpinBox(&dlg); wSpin->setRange(1, 16384); wSpin->setValue(origW);
    auto* hSpin = new QSpinBox(&dlg); hSpin->setRange(1, 16384); hSpin->setValue(origH);
    auto* lockAR = new QCheckBox(tr("Keep aspect ratio"), &dlg); lockAR->setChecked(true);

    form->addRow(tr("Width:"),  wSpin);
    form->addRow(tr("Height:"), hSpin);
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

// ── Magic Select / segmentation ───────────────────────────────────────────────

void MainWindow::clearSelection() {
    m_selectionMask.clear();
    m_imageLabel->clearSelectionOverlay();
    m_actExportSelection->setEnabled(false);
    if (m_statusSelection) m_statusSelection->clear();
}

void MainWindow::updateSelectionStatus() {
    int n = SegmentTool::countSelected(m_selectionMask);
    if (n > 0)
        m_statusSelection->setText(QString("  %1 px selected").arg(n));
    else
        m_statusSelection->clear();
    m_actExportSelection->setEnabled(n > 0);
}

void MainWindow::onMagicSelectToggled(bool on) {
    m_imageLabel->setTool(on ? ImageLabel::Tool::MagicSelect : ImageLabel::Tool::None);
    m_thresholdAction->setVisible(on);
    if (!on) {
        clearSelection();
    }
}

void MainWindow::onMagicSelectClicked(QPoint pt) {
    if (m_currentImage.is_empty()) return;
    // Clamp to image bounds
    pt.setX(std::clamp(pt.x(), 0, m_currentImage.width()  - 1));
    pt.setY(std::clamp(pt.y(), 0, m_currentImage.height() - 1));
    m_lastMagicSeed  = pt;
    m_selectionMask  = SegmentTool::regionGrow(
        m_currentImage, pt.x(), pt.y(), m_selectThreshold);
    QImage overlay   = SegmentTool::makeOverlay(
        m_selectionMask, m_currentImage.width(), m_currentImage.height());
    m_imageLabel->setSelectionOverlay(overlay);
    updateSelectionStatus();
}

void MainWindow::onThresholdChanged(int value) {
    m_selectThreshold = value;
    m_thresholdLabel->setText(tr("Threshold: %1").arg(value));
    // Re-run region growing if a seed exists
    if (!m_selectionMask.empty())
        onMagicSelectClicked(m_lastMagicSeed);
}

void MainWindow::onExportSelection() {
    if (m_selectionMask.empty() || m_currentImage.is_empty()) return;

    QString path = QFileDialog::getSaveFileName(this, tr("Export Selection"),
        QDir::homePath() + "/selection.png",
        tr("PNG Images (*.png)"));
    if (path.isEmpty()) return;

    int W = m_currentImage.width(), H = m_currentImage.height();
    QImage result(W, H, QImage::Format_ARGB32);
    result.fill(Qt::transparent);

    int C = m_currentImage.spectrum();
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (!m_selectionMask[y * W + x]) continue;
            int r = m_currentImage(x, y, 0, 0);
            int g = (C > 1) ? m_currentImage(x, y, 0, 1) : r;
            int b = (C > 2) ? m_currentImage(x, y, 0, 2) : r;
            result.setPixel(x, y, qRgba(r, g, b, 255));
        }
    }

    if (!result.save(path, "PNG"))
        QMessageBox::warning(this, tr("Export Error"),
            tr("Could not save selection to:\n") + path);
    else
        statusBar()->showMessage(tr("Selection exported: ") + path, 4000);
}

// ── AI operations ─────────────────────────────────────────────────────────────

void MainWindow::onAiRemoveBg() {
    if (m_currentImage.is_empty()) return;
    AiTools::removeBackground(m_currentImage, this,
        [this](CImgU8 r) {
            applyOperation("Remove Background", r);

            // Prompt to save — only PNG preserves the alpha channel
            QString suggestion = m_currentFilePath.isEmpty()
                ? QDir::homePath() + "/bg_removed.png"
                : QFileInfo(m_currentFilePath).dir().filePath(
                      QFileInfo(m_currentFilePath).completeBaseName() + "_nobg.png");

            QString path = QFileDialog::getSaveFileName(
                this,
                tr("Save PNG with Transparent Background"),
                suggestion,
                tr("PNG Image (*.png)"));
            if (path.isEmpty()) return;

            // Ensure .png extension
            if (!path.endsWith(".png", Qt::CaseInsensitive)) path += ".png";

            QString err;
            if (FileManager::saveImage(r, path, &err)) {
                LOG_INFO("MainWindow",
                    QString("RMBG result saved: %1").arg(path));
                statusBar()->showMessage(
                    tr("Saved: %1").arg(QFileInfo(path).fileName()), 5000);
            } else {
                QMessageBox::warning(this, tr("Save Error"), err);
            }
        },
        [this](QString e) { QMessageBox::critical(this, tr("AI Error"), e); });
}

void MainWindow::onAiSuperRes() {
    if (m_currentImage.is_empty()) return;
    AiTools::superResolution(m_currentImage, this,
        [this](CImgU8 r) { applyOperation("Super Resolution ×4", r); },
        [this](QString e) { QMessageBox::critical(this, tr("AI Error"), e); });
}

void MainWindow::onAiDetect() {
    if (m_currentImage.is_empty()) return;
    AiTools::detectObjects(m_currentImage, this,
        [this](QVector<QPair<QRectF,QString>> boxes) {
            // Draw boxes on annotation layer
            if (!m_actAnnotate->isChecked())
                m_actAnnotate->setChecked(true);
            m_annotLayer->setColor(Qt::red);
            m_annotLayer->setLineWidth(2);
            QMap<QString,int> counts;
            for (const auto& [rect, label] : boxes) {
                m_annotLayer->setDrawTool(AnnotationLayer::DrawTool::Rectangle);
                m_annotLayer->pressAt(rect.topLeft());
                m_annotLayer->releaseAt(rect.bottomRight());
                m_annotLayer->setDrawTool(AnnotationLayer::DrawTool::Text);
                m_annotLayer->setFontSize(12);
                m_annotLayer->pressAt(rect.topLeft() - QPointF(0, 2));
                m_annotLayer->releaseAt(rect.topLeft() - QPointF(0, 2), label);
                counts[label]++;
            }
            updateAnnotationOverlay();
            updateAnnotationActions();
            // Status bar summary
            QStringList summary;
            for (auto it = counts.begin(); it != counts.end(); ++it)
                summary << QString("%1 %2").arg(it.value()).arg(it.key());
            statusBar()->showMessage(
                tr("%1 object(s) detected: %2")
                    .arg(boxes.size()).arg(summary.join(", ")), 8000);
        },
        [this](QString e) { QMessageBox::critical(this, tr("AI Error"), e); });
}

void MainWindow::onAiStyleTransfer(const QString& styleId) {
    if (m_currentImage.is_empty()) return;
    QString styleName = styleId.mid(styleId.lastIndexOf('_') + 1);
    styleName[0] = styleName[0].toUpper();
    AiTools::styleTransfer(m_currentImage, styleId, this,
        [this, styleName](CImgU8 r) {
            applyOperation("Style: " + styleName, r);
        },
        [this](QString e) { QMessageBox::critical(this, tr("AI Error"), e); });
}

void MainWindow::onColorizeLabTransfer() {
    if (m_currentImage.is_empty()) return;

    // Pick reference image
    QString refPath = QFileDialog::getOpenFileName(
        this, tr("Choose Reference (Colour) Image"), QString(),
        tr("Images (*.jpg *.jpeg *.png *.bmp *.tiff *.tif)"));
    if (refPath.isEmpty()) return;

    CImgU8 reference;
    try {
        reference.load(refPath.toStdString().c_str());
    } catch (...) {
        QMessageBox::warning(this, tr("Colorize"), tr("Could not load reference image."));
        return;
    }

    // Intensity slider dialog
    QDialog dlg(this);
    dlg.setWindowTitle(tr("LAB Colour Transfer"));
    auto* vl   = new QVBoxLayout(&dlg);
    auto* lbl  = new QLabel(tr("Transfer intensity:"), &dlg);
    auto* sld  = new QSlider(Qt::Horizontal, &dlg);
    sld->setRange(1, 100);
    sld->setValue(100);
    auto* valLbl = new QLabel("100%", &dlg);
    QObject::connect(sld, &QSlider::valueChanged, valLbl,
                     [valLbl](int v){ valLbl->setText(QString::number(v) + "%"); });
    auto* hl  = new QHBoxLayout;
    hl->addWidget(sld); hl->addWidget(valLbl);
    vl->addWidget(lbl);
    vl->addLayout(hl);
    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    vl->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted) return;

    float intensity = sld->value() / 100.f;
    LOG_INFO("MainWindow",
        QString("LAB colour transfer: intensity=%.2f ref=%1").arg(refPath).arg(intensity));

    CImgU8 result = ImageProcessor::labColorTransfer(m_currentImage, reference, intensity);
    applyOperation("Colorize (LAB)", result);
}

void MainWindow::onAiManageModels() {
    ModelManagerDialog dlg(this);
    dlg.exec();
}

// ── Annotation layer ──────────────────────────────────────────────────────────

QString MainWindow::annotationJsonPath(const QString& imagePath) const {
    QFileInfo fi(imagePath);
    return fi.absolutePath() + "/" + fi.completeBaseName() + ".annotations.json";
}

void MainWindow::loadAnnotationsForImage(const QString& imagePath) {
    QString jsonPath = annotationJsonPath(imagePath);
    QFile f(jsonPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isNull())
        m_annotLayer->fromJson(doc.object());
}

void MainWindow::updateAnnotationOverlay() {
    if (!m_currentImage.is_empty() && !m_annotLayer->isEmpty()) {
        QImage overlay = m_annotLayer->renderToImage(
            m_currentImage.width(), m_currentImage.height());
        m_imageLabel->setAnnotationOverlay(overlay);
    } else if (!m_currentImage.is_empty() && m_annotLayer->isEmpty()) {
        m_imageLabel->clearAnnotationOverlay();
    }
}

void MainWindow::updateAnnotationActions() {
    if (m_actAnnotUndo) m_actAnnotUndo->setEnabled(m_annotLayer->canUndo());
    if (m_actAnnotRedo) m_actAnnotRedo->setEnabled(m_annotLayer->canRedo());
}

void MainWindow::onAnnotateToggled(bool on) {
    m_annotToolBar->setVisible(on);
    m_imageLabel->setTool(on ? ImageLabel::Tool::Annotate : ImageLabel::Tool::None);
    if (on && m_annotLayer) {
        m_annotLayer->setColor(Qt::red);
        m_annotLayer->setLineWidth(m_annotLineWidthSpin->value());
        m_annotLayer->setFontSize(m_annotFontSizeSpin->value());
    }
}

void MainWindow::onAnnotatePress(QPointF pt) {
    if (!m_annotLayer || m_currentImage.is_empty()) return;
    m_annotLayer->pressAt(pt);
    updateAnnotationOverlay();
    updateAnnotationActions();
}

void MainWindow::onAnnotateMove(QPointF pt) {
    if (!m_annotLayer || m_currentImage.is_empty()) return;
    m_annotLayer->moveAt(pt);
    // Update overlay during drag for live preview
    if (!m_currentImage.is_empty()) {
        QImage overlay = m_annotLayer->renderToImage(
            m_currentImage.width(), m_currentImage.height());
        m_imageLabel->setAnnotationOverlay(overlay);
    }
}

void MainWindow::onAnnotateRelease(QPointF pt) {
    if (!m_annotLayer || m_currentImage.is_empty()) return;

    // For text tool, prompt for input before committing
    if (m_annotLayer->drawTool() == AnnotationLayer::DrawTool::Text) {
        bool ok;
        QString text = QInputDialog::getText(this, tr("Add Text Annotation"),
            tr("Text:"), QLineEdit::Normal, "", &ok);
        m_annotLayer->releaseAt(pt, ok ? text : QString());
    } else {
        m_annotLayer->releaseAt(pt);
    }
    updateAnnotationOverlay();
    updateAnnotationActions();
}

void MainWindow::onAnnotateUndo() {
    if (!m_annotLayer) return;
    m_annotLayer->undo();
    updateAnnotationOverlay();
    updateAnnotationActions();
}

void MainWindow::onAnnotateRedo() {
    if (!m_annotLayer) return;
    m_annotLayer->redo();
    updateAnnotationOverlay();
    updateAnnotationActions();
}

void MainWindow::onAnnotateColor() {
    if (!m_annotLayer) return;
    QColor c = QColorDialog::getColor(m_annotLayer->color(), this, tr("Annotation Colour"));
    if (c.isValid()) {
        m_annotLayer->setColor(c);
        // Update the color button text to reflect new color
        if (m_actAnnotColor)
            m_actAnnotColor->setToolTip(QString("Color: %1").arg(c.name()));
    }
}

void MainWindow::onSaveWithAnnotations() {
    if (m_currentImage.is_empty()) return;
    QString path = QFileDialog::getSaveFileName(this, tr("Save with Annotations"),
        m_currentFilePath.isEmpty() ? QDir::homePath() : m_currentFilePath,
        tr("Images (*.png *.jpg *.jpeg *.bmp);;All files (*)"));
    if (path.isEmpty()) return;

    QImage base = ImageConvert::toQImage(m_currentImage);
    QImage flat = m_annotLayer->flattenOnto(base);
    if (!flat.save(path))
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not save image with annotations to:\n") + path);
    else
        statusBar()->showMessage(tr("Saved with annotations: ") + path, 4000);
}

void MainWindow::onSaveWithoutAnnotations() {
    onSave();  // normal save, annotations are not composited
}

void MainWindow::onSaveAnnotationsJson() {
    QString defaultPath = m_currentFilePath.isEmpty()
        ? QDir::homePath() + "/annotations.json"
        : annotationJsonPath(m_currentFilePath);
    QString path = QFileDialog::getSaveFileName(this, tr("Save Annotations"),
        defaultPath, tr("JSON files (*.json);;All files (*)"));
    if (path.isEmpty()) return;

    QJsonDocument doc(m_annotLayer->toJson());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(doc.toJson()) < 0) {
        LOG_ERROR("MainWindow", QString("Failed to save annotations JSON: %1").arg(path));
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not save annotations to:\n") + path);
    } else {
        LOG_INFO("MainWindow", QString("Annotations saved: %1").arg(path));
        statusBar()->showMessage(tr("Annotations saved: ") + path, 4000);
    }
}

// ── File browser ──────────────────────────────────────────────────────────────

void MainWindow::onFileBrowserClicked(const QModelIndex& idx) {
    QString path = m_fsModel->filePath(idx);
    if (!m_fsModel->isDir(idx)) loadImage(path);
}

// ── Help ─────────────────────────────────────────────────────────────────────

void MainWindow::onKeyboardShortcuts() {
    QMessageBox::information(this, tr("Keyboard Shortcuts"),
        tr("File\n"
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
        "  C               Crop (draw rectangle)")
    );
}

void MainWindow::onOpenLogFile() {
    QString path = Logger::instance().currentLogPath();
    LOG_INFO("MainWindow", "Opening log file: " + path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::onOpenLogFolder() {
    QString dir = Logger::instance().logDir();
    LOG_INFO("MainWindow", "Opening log folder: " + dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::onAbout() {
    QString aiProvider = OnnxEngine::instance().providerName();
    QMessageBox::about(this, tr("About CImg Viewer"),
        tr("<b>CImg Viewer</b><br><br>"
        "A cross-platform image viewer and editor built with:<br>"
        "&bull; <b>CImg</b> (header-only image processing)<br>"
        "&bull; <b>Qt 6</b> (GUI framework)<br>"
        "&bull; <b>ONNX Runtime 1.17.1</b> (AI inference)<br>")
        + "   AI Engine: <b>" + aiProvider + "</b><br><br>"
        + tr("Based on the book:<br>"
        "<i>Digital Image Processing with C++: Implementing Reference Algorithms "
        "with the CImg Library</i><br>"
        "Tschumperlé, Tilmant, Barra — CRC Press, 2023")
    );
}

// ── Language / Settings ───────────────────────────────────────────────────────

void MainWindow::onLanguagePtBr() {
    QSettings s("CImgViewer", "CImgViewer");
    s.setValue("language", "pt_BR");
    QMessageBox::information(this, tr("Language"),
        "O idioma será aplicado ao reiniciar o programa.");
}

void MainWindow::onLanguageEn() {
    QSettings s("CImgViewer", "CImgViewer");
    s.setValue("language", "en");
    QMessageBox::information(this, tr("Language"),
        "Language will be applied on restart.");
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
    const QStringList filters = FileManager::supportedExtensions();
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
        m_actSlideshow->setToolTip(tr("Stop Slideshow (F5)"));
    } else {
        m_slideshowTimer->stop();
        m_slideshowCombo->setVisible(false);
        m_actSlideshow->setIcon(QIcon::fromTheme("media-playback-start"));
        m_actSlideshow->setToolTip(tr("Start Slideshow (F5)"));
    }
    updateStatusBar();
}

void MainWindow::onSlideshowTick() {
    onNextImage();
}

// ── Keyboard ─────────────────────────────────────────────────────────────────

void MainWindow::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        if (m_actCrop->isChecked()) {
            m_actCrop->setChecked(false);
            m_imageLabel->setTool(ImageLabel::Tool::None);
            return;
        }
        if (m_actMagicSelect->isChecked()) {
            m_actMagicSelect->setChecked(false);
            return;
        }
        if (!m_selectionMask.empty()) {
            clearSelection();
            return;
        }
        if (m_actSlideshow->isChecked()) {
            m_actSlideshow->setChecked(false);
            return;
        }
    }
    QMainWindow::keyPressEvent(e);
}

// ── Event filter (pan) ────────────────────────────────────────────────────────

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_scrollArea->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_panning = true;
                m_panStart = me->globalPosition().toPoint();
                m_scrollArea->viewport()->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (m_panning) {
                QPoint delta = me->globalPosition().toPoint() - m_panStart;
                m_panStart = me->globalPosition().toPoint();
                m_scrollArea->horizontalScrollBar()->setValue(
                    m_scrollArea->horizontalScrollBar()->value() - delta.x());
                m_scrollArea->verticalScrollBar()->setValue(
                    m_scrollArea->verticalScrollBar()->value() - delta.y());
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && m_panning) {
                m_panning = false;
                m_scrollArea->viewport()->setCursor(Qt::OpenHandCursor);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
