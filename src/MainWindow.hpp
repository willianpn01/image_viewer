#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QScrollArea>
#include <QTreeView>
#include <QFileSystemModel>
#include <QDockWidget>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QSlider>
#include <QStatusBar>
#include <QTimer>
#include <QComboBox>
#include <QStringList>

#include "ImageProcessor.hpp"   // defines CImgU8
#include "UndoStack.hpp"
#include "HistogramWidget.hpp"
#include "ImageLabel.hpp"
#include "ImageConvert.hpp"
#include "CompareView.hpp"
#include "ExifPanel.hpp"
#include "PanHandler.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Open a file path from the command line or drag-and-drop
    void openPath(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void closeEvent(QCloseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    // ── File ──────────────────────────────────────────────────────────────────
    void onOpenFile();
    void onOpenFolder();
    void onSave();
    void onSaveAs();
    void onOpenRecent();
    void onClearRecent();

    // ── Edit ──────────────────────────────────────────────────────────────────
    void onUndo();
    void onRedo();

    // ── View ──────────────────────────────────────────────────────────────────
    void onZoomIn();
    void onZoomOut();
    void onFitToWindow(bool checked);
    void onActualSize();

    // ── Folder navigation / slideshow ─────────────────────────────────────────
    void onPrevImage();
    void onNextImage();
    void onSlideshow(bool checked);
    void onSlideshowTick();

    // ── Compare mode ──────────────────────────────────────────────────────────
    void onCompareMode(bool checked);

    // ── Image / Adjustments ───────────────────────────────────────────────────
    void onBrightness();
    void onContrast();
    void onGamma();
    void onGrayscale();
    void onNegative();
    void onHistogramEq();

    // ── Image / Filters ───────────────────────────────────────────────────────
    void onGaussianBlur();
    void onMedianBlur();
    void onSharpen();
    void onSobel();
    void onCanny();
    void onEmboss();

    // ── Image / Transform ─────────────────────────────────────────────────────
    void onRotateCW();
    void onRotateCCW();
    void onFlipH();
    void onFlipV();
    void onCrop();
    void performCrop(QRect imageRect);
    void onResize();

    // ── Image / Morphology ────────────────────────────────────────────────────
    void onErode();
    void onDilate();
    void onMorphOpen();
    void onMorphClose();
    void onMorphGradient();

    // ── Help ──────────────────────────────────────────────────────────────────
    void onKeyboardShortcuts();
    void onAbout();

    // ── Misc ──────────────────────────────────────────────────────────────────
    void onFileBrowserClicked(const QModelIndex& idx);

private:
    // ── Setup helpers ─────────────────────────────────────────────────────────
    void createActions();
    void createMenus();
    void createToolBar();
    void createFileBrowserDock();
    void createHistogramDock();
    void createExifDock();
    void createStatusBarWidgets();

    // ── Image helpers ─────────────────────────────────────────────────────────
    void loadImage(const QString& path);
    void displayImage();            // Renders m_currentImage with current zoom
    void applyOperation(const QString& name, const CImgU8& result);
    void updateStatusBar();
    void updateActions();
    void updateHistogram();
    void setZoom(double z);

    // ── Recent files ──────────────────────────────────────────────────────────
    void rebuildRecentMenu();

    // ── Folder navigation helpers ─────────────────────────────────────────────
    void buildFolderList(const QString& dir);
    void navigateToIndex(int idx);
    int  currentFolderIndex() const;

    // ── Settings ──────────────────────────────────────────────────────────────
    void saveSettings();
    void loadSettings();

    // ── Data ──────────────────────────────────────────────────────────────────
    CImgU8      m_currentImage;
    QString     m_currentFilePath;
    double      m_zoom         = 1.0;
    bool        m_fitToWindow  = false;
    UndoStack   m_undoStack;

    // ── Widgets ───────────────────────────────────────────────────────────────
    ImageLabel*       m_imageLabel      = nullptr;
    QScrollArea*      m_scrollArea      = nullptr;
    HistogramWidget*  m_histWidget      = nullptr;
    QDockWidget*      m_fileDock        = nullptr;
    QDockWidget*      m_histDock        = nullptr;
    QTreeView*        m_fileTree        = nullptr;
    QFileSystemModel* m_fsModel         = nullptr;
    QLabel*           m_statusImgInfo   = nullptr;
    QLabel*           m_statusZoom      = nullptr;
    QLabel*           m_statusPath      = nullptr;

    // ── Compare mode ──────────────────────────────────────────────────────────
    CompareView*      m_compareView     = nullptr;
    CImgU8            m_originalImage;   // frozen at load time for compare
    QAction*          m_actCompare      = nullptr;

    // ── EXIF panel ────────────────────────────────────────────────────────────
    ExifPanel*        m_exifPanel       = nullptr;
    QDockWidget*      m_exifDock        = nullptr;

    // ── Pan handler ───────────────────────────────────────────────────────────
    PanHandler*       m_panHandler      = nullptr;

    // ── Actions ───────────────────────────────────────────────────────────────
    QAction* m_actOpen        = nullptr;
    QAction* m_actOpenFolder  = nullptr;
    QAction* m_actSave        = nullptr;
    QAction* m_actSaveAs      = nullptr;
    QAction* m_actUndo        = nullptr;
    QAction* m_actRedo        = nullptr;
    QAction* m_actZoomIn      = nullptr;
    QAction* m_actZoomOut     = nullptr;
    QAction* m_actFitWindow   = nullptr;
    QAction* m_actActualSize  = nullptr;
    QAction* m_actCrop        = nullptr;
    QAction* m_actPrev        = nullptr;
    QAction* m_actNext        = nullptr;
    QAction* m_actSlideshow   = nullptr;

    // Image actions (enabled only when image loaded)
    QList<QAction*> m_imageActions;

    // Recent files menu
    QMenu* m_recentMenu = nullptr;
    QList<QAction*> m_recentActions;

    // ── Folder navigation / slideshow ─────────────────────────────────────────
    QStringList m_folderImages;    // sorted image paths in current folder
    QTimer*     m_slideshowTimer  = nullptr;
    QComboBox*  m_slideshowCombo  = nullptr;  // interval selector in toolbar
    QAction*    m_slideshowComboAction = nullptr;

    // Preferences (UndoStack depth)
    int m_maxUndoDepth = 20;
};
