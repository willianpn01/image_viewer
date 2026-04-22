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
#include <QHBoxLayout>
#include <QSpinBox>
#include <QActionGroup>
#include <QTimer>
#include <QComboBox>
#include <QStringList>
#include <QListWidget>
#include <QStandardPaths>

#include "ImageProcessor.hpp"   // defines CImgU8
#include "UndoStack.hpp"
#include "HistogramWidget.hpp"
#include "ImageLabel.hpp"
#include "ImageConvert.hpp"
#include "CompareView.hpp"
#include "ExifPanel.hpp"
#include "SegmentTool.hpp"
#include "AnnotationLayer.hpp"
#include "AiTools.hpp"
#include <vector>
#include <memory>
#include <functional>

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
    bool eventFilter(QObject* obj, QEvent* event) override;

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
    void onNonLinearDiffusion();

    // ── Tools ─────────────────────────────────────────────────────────────────
    void onMagicSelectToggled(bool on);
    void onMagicSelectClicked(QPoint imagePoint);
    void onExportSelection();
    void onThresholdChanged(int value);

    // ── AI operations ─────────────────────────────────────────────────────────
    void onAiRemoveBg();
    void onAiSuperRes();
    void onAiDetect();
    void onAiStyleTransfer(const QString& styleId);
    void onAiManageModels();

    // ── Image / Colorize (LAB, no ONNX) ──────────────────────────────────────
    void onColorizeLabTransfer();

    // ── Annotations ───────────────────────────────────────────────────────────
    void onAnnotateToggled(bool on);
    void onAnnotatePress(QPointF pt);
    void onAnnotateMove(QPointF pt);
    void onAnnotateRelease(QPointF pt);
    void onAnnotateUndo();
    void onAnnotateRedo();
    void onAnnotateColor();
    void onSaveWithAnnotations();
    void onSaveWithoutAnnotations();
    void onSaveAnnotationsJson();

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
    void onOpenLogFile();
    void onOpenLogFolder();

    // ── Settings / Language ───────────────────────────────────────────────────
    void onLanguagePtBr();
    void onLanguageEn();

    // ── Misc ──────────────────────────────────────────────────────────────────
    void onFileBrowserClicked(const QModelIndex& idx);

    // ── File browser favorites ─────────────────────────────────────────────────
    void onFavItemClicked(QListWidgetItem* item);
    void onFavContextMenu(const QPoint& pos);
    void onTreeContextMenu(const QPoint& pos);

private:
    // ── Setup helpers ─────────────────────────────────────────────────────────
    void createActions();
    void createMenus();
    void createToolBar();
    void createAnnotationToolBar();
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

    // ── File browser helpers ──────────────────────────────────────────────────
    void populateFavorites();
    void navigateFileBrowserTo(const QString& dir);

    // ── Settings ──────────────────────────────────────────────────────────────
    void saveSettings();
    void loadSettings();

    // ── Selection helpers ─────────────────────────────────────────────────────
    void clearSelection();
    void updateSelectionStatus();

    // ── Annotation helpers ────────────────────────────────────────────────────
    void updateAnnotationOverlay();
    void updateAnnotationActions();
    void loadAnnotationsForImage(const QString& imagePath);
    QString annotationJsonPath(const QString& imagePath) const;

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
    QListWidget*      m_favList         = nullptr;
    QLabel*           m_statusImgInfo   = nullptr;
    QLabel*           m_statusZoom      = nullptr;
    QLabel*           m_statusPath      = nullptr;
    QLabel*           m_statusSelection = nullptr;

    // ── Compare mode ──────────────────────────────────────────────────────────
    CompareView*      m_compareView     = nullptr;
    CImgU8            m_originalImage;   // frozen at load time for compare
    QAction*          m_actCompare      = nullptr;

    // ── EXIF panel ────────────────────────────────────────────────────────────
    ExifPanel*        m_exifPanel       = nullptr;
    QDockWidget*      m_exifDock        = nullptr;

    // ── Pan state ─────────────────────────────────────────────────────────────
    bool   m_panning  = false;
    QPoint m_panStart;

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

    // ── Magic Select / segmentation ───────────────────────────────────────────
    std::vector<bool>  m_selectionMask;
    QPoint             m_lastMagicSeed;
    int                m_selectThreshold = 30;

    QAction*  m_actMagicSelect      = nullptr;
    QAction*  m_actExportSelection  = nullptr;
    QSlider*  m_thresholdSlider     = nullptr;
    QLabel*   m_thresholdLabel      = nullptr;
    QAction*  m_thresholdAction     = nullptr;  // widget-action in toolbar

    // ── Annotation layer ──────────────────────────────────────────────────────
    std::unique_ptr<AnnotationLayer> m_annotLayer;
    QAction*    m_actAnnotate        = nullptr;
    QToolBar*   m_annotToolBar       = nullptr;  // secondary toolbar
    QAction*    m_actAnnotUndo       = nullptr;
    QAction*    m_actAnnotRedo       = nullptr;
    QAction*    m_actAnnotColor      = nullptr;
    QSpinBox*   m_annotLineWidthSpin = nullptr;
    QSpinBox*   m_annotFontSizeSpin  = nullptr;
    QActionGroup* m_annotToolGroup   = nullptr;
    QAction*    m_actSaveWithAnnot   = nullptr;
    QAction*    m_actSaveWithoutAnnot= nullptr;
    QAction*    m_actSaveAnnotJson   = nullptr;
};


