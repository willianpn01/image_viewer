#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <atomic>

// Batch-processes all images in a source folder and writes them to a
// destination folder. Runs synchronously in the UI thread to avoid Qt
// threading complexity, but updates the progress bar via processEvents().
class BatchExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit BatchExportDialog(QWidget* parent = nullptr);

private slots:
    void onBrowseIn();
    void onBrowseOut();
    void onFormatChanged(int idx);
    void onStart();
    void onCancel();

private:
    // ── UI ────────────────────────────────────────────────────────────────────
    QLineEdit*    m_inDir        = nullptr;
    QLineEdit*    m_outDir       = nullptr;
    QComboBox*    m_fmtCombo     = nullptr;
    QSlider*      m_jpgQuality   = nullptr;
    QLabel*       m_jpgQLabel    = nullptr;
    QWidget*      m_jpgRow       = nullptr;

    QCheckBox*    m_chkResize    = nullptr;
    QSpinBox*     m_resizeSpin   = nullptr;
    QCheckBox*    m_chkGray      = nullptr;
    QCheckBox*    m_chkBright    = nullptr;
    QSlider*      m_brightSlider = nullptr;
    QLabel*       m_brightLabel  = nullptr;

    QProgressBar* m_progress     = nullptr;
    QLabel*       m_statusLabel  = nullptr;
    QPushButton*  m_btnStart     = nullptr;
    QPushButton*  m_btnCancel    = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    std::atomic<bool> m_cancelFlag{false};
    bool              m_running = false;

    void setRunning(bool r);
};
