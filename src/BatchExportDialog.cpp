#include "BatchExportDialog.hpp"
#include "FileManager.hpp"
#include "ImageProcessor.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageWriter>

BatchExportDialog::BatchExportDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Batch Export");
    setMinimumWidth(480);

    auto* mainLayout = new QVBoxLayout(this);

    // ── I/O folders ───────────────────────────────────────────────────────────
    auto* ioGroup = new QGroupBox("Folders", this);
    auto* ioForm  = new QFormLayout(ioGroup);

    m_inDir  = new QLineEdit(this);
    m_outDir = new QLineEdit(this);

    auto* btnIn  = new QPushButton("…", this); btnIn->setFixedWidth(30);
    auto* btnOut = new QPushButton("…", this); btnOut->setFixedWidth(30);

    auto* inRow  = new QHBoxLayout; inRow->addWidget(m_inDir);  inRow->addWidget(btnIn);
    auto* outRow = new QHBoxLayout; outRow->addWidget(m_outDir); outRow->addWidget(btnOut);

    ioForm->addRow("Input folder:",  inRow);
    ioForm->addRow("Output folder:", outRow);
    mainLayout->addWidget(ioGroup);

    connect(btnIn,  &QPushButton::clicked, this, &BatchExportDialog::onBrowseIn);
    connect(btnOut, &QPushButton::clicked, this, &BatchExportDialog::onBrowseOut);

    // ── Output format ─────────────────────────────────────────────────────────
    auto* fmtGroup = new QGroupBox("Output Format", this);
    auto* fmtForm  = new QFormLayout(fmtGroup);

    m_fmtCombo = new QComboBox(this);
    m_fmtCombo->addItem("PNG",  "png");
    m_fmtCombo->addItem("JPG",  "jpg");
    m_fmtCombo->addItem("BMP",  "bmp");
    m_fmtCombo->addItem("TIFF", "tiff");
    m_fmtCombo->addItem("WebP", "webp");
    m_fmtCombo->addItem("TGA",  "tga");
    fmtForm->addRow("Format:", m_fmtCombo);

    // JPG quality row
    m_jpgRow   = new QWidget(this);
    auto* jpgL = new QHBoxLayout(m_jpgRow);
    jpgL->setContentsMargins(0,0,0,0);
    m_jpgQuality = new QSlider(Qt::Horizontal, this);
    m_jpgQuality->setRange(1, 100);
    m_jpgQuality->setValue(90);
    m_jpgQLabel  = new QLabel("90", this);
    m_jpgQLabel->setMinimumWidth(30);
    jpgL->addWidget(m_jpgQuality);
    jpgL->addWidget(m_jpgQLabel);
    fmtForm->addRow(tr("Quality:"), m_jpgRow);
    m_jpgRow->setVisible(false);

    mainLayout->addWidget(fmtGroup);

    connect(m_fmtCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BatchExportDialog::onFormatChanged);
    connect(m_jpgQuality, &QSlider::valueChanged, this, [this](int v) {
        m_jpgQLabel->setText(QString::number(v));
    });

    // ── Operations ────────────────────────────────────────────────────────────
    auto* opsGroup = new QGroupBox("Operations", this);
    auto* opsLayout = new QVBoxLayout(opsGroup);

    // Resize
    auto* resizeRow = new QHBoxLayout;
    m_chkResize  = new QCheckBox("Resize to max width:", this);
    m_resizeSpin = new QSpinBox(this);
    m_resizeSpin->setRange(1, 16384);
    m_resizeSpin->setValue(1920);
    m_resizeSpin->setSuffix(" px");
    m_resizeSpin->setEnabled(false);
    resizeRow->addWidget(m_chkResize);
    resizeRow->addWidget(m_resizeSpin);
    resizeRow->addStretch();
    opsLayout->addLayout(resizeRow);

    connect(m_chkResize, &QCheckBox::toggled, m_resizeSpin, &QSpinBox::setEnabled);

    // Grayscale
    m_chkGray = new QCheckBox("Convert to grayscale", this);
    opsLayout->addWidget(m_chkGray);

    // Brightness
    auto* brightRow = new QHBoxLayout;
    m_chkBright   = new QCheckBox("Apply brightness:", this);
    m_brightSlider = new QSlider(Qt::Horizontal, this);
    m_brightSlider->setRange(-100, 100);
    m_brightSlider->setValue(0);
    m_brightSlider->setEnabled(false);
    m_brightLabel  = new QLabel("0", this);
    m_brightLabel->setMinimumWidth(30);
    brightRow->addWidget(m_chkBright);
    brightRow->addWidget(m_brightSlider);
    brightRow->addWidget(m_brightLabel);
    opsLayout->addLayout(brightRow);

    connect(m_chkBright, &QCheckBox::toggled, m_brightSlider, &QSlider::setEnabled);
    connect(m_brightSlider, &QSlider::valueChanged, this, [this](int v) {
        m_brightLabel->setText(QString::number(v));
    });

    mainLayout->addWidget(opsGroup);

    // ── Progress ──────────────────────────────────────────────────────────────
    m_progress    = new QProgressBar(this);
    m_progress->setValue(0);
    m_statusLabel = new QLabel("Ready.", this);
    mainLayout->addWidget(m_progress);
    mainLayout->addWidget(m_statusLabel);

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    m_btnStart  = new QPushButton("Start", this);
    m_btnCancel = new QPushButton("Close", this);
    btnRow->addStretch();
    btnRow->addWidget(m_btnStart);
    btnRow->addWidget(m_btnCancel);
    mainLayout->addLayout(btnRow);

    connect(m_btnStart,  &QPushButton::clicked, this, &BatchExportDialog::onStart);
    connect(m_btnCancel, &QPushButton::clicked, this, &BatchExportDialog::onCancel);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void BatchExportDialog::onBrowseIn() {
    QString d = QFileDialog::getExistingDirectory(this, "Select Input Folder",
                                                   m_inDir->text());
    if (!d.isEmpty()) m_inDir->setText(d);
}

void BatchExportDialog::onBrowseOut() {
    QString d = QFileDialog::getExistingDirectory(this, "Select Output Folder",
                                                   m_outDir->text());
    if (!d.isEmpty()) m_outDir->setText(d);
}

void BatchExportDialog::onFormatChanged(int) {
    QString fmt = m_fmtCombo->currentData().toString();
    // Quality slider applies to lossy formats (JPG and WebP)
    m_jpgRow->setVisible(fmt == "jpg" || fmt == "webp");
}

void BatchExportDialog::setRunning(bool r) {
    m_running = r;
    m_btnStart->setEnabled(!r);
    m_btnCancel->setText(r ? "Cancel" : "Close");
}

void BatchExportDialog::onCancel() {
    if (m_running) {
        m_cancelFlag.store(true);
    } else {
        accept();
    }
}

void BatchExportDialog::onStart() {
    QString inPath  = m_inDir->text().trimmed();
    QString outPath = m_outDir->text().trimmed();

    if (inPath.isEmpty() || outPath.isEmpty()) {
        QMessageBox::warning(this, "Batch Export", "Please specify input and output folders.");
        return;
    }
    QDir outDir(outPath);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        QMessageBox::warning(this, "Batch Export",
                             "Cannot create output folder: " + outPath);
        return;
    }

    static const QStringList filters = FileManager::supportedExtensions();
    QDir inDir(inPath);
    QStringList files = inDir.entryList(filters, QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        QMessageBox::information(this, "Batch Export", "No images found in input folder.");
        return;
    }

    QString ext  = m_fmtCombo->currentData().toString();
    // Quality applies to JPG and WebP; -1 means Qt picks default for other formats
    int quality = (ext == "jpg" || ext == "webp") ? m_jpgQuality->value() : -1;
    bool doResize = m_chkResize->isChecked();
    int  maxW     = m_resizeSpin->value();
    bool doGray   = m_chkGray->isChecked();
    bool doBright = m_chkBright->isChecked();
    int  bright   = m_brightSlider->value();

    m_cancelFlag.store(false);
    setRunning(true);
    m_progress->setRange(0, files.size());
    m_progress->setValue(0);

    int processed = 0, failed = 0;

    for (const QString& fname : files) {
        if (m_cancelFlag.load()) break;

        QString srcPath = inDir.absoluteFilePath(fname);
        m_statusLabel->setText(QString("Processing %1/%2: %3")
            .arg(processed + 1).arg(files.size()).arg(fname));
        QApplication::processEvents();

        // Load via Qt
        QString err;
        CImgU8 img = FileManager::loadImage(srcPath, &err);
        if (img.is_empty()) { ++failed; ++processed; m_progress->setValue(processed); continue; }

        // Apply operations
        if (doGray)   img = ImageProcessor::toGrayscale(img);
        if (doBright) img = ImageProcessor::brightness(img, bright);
        if (doResize && img.width() > maxW) {
            int newH = (int)std::round((double)img.height() / img.width() * maxW);
            img = ImageProcessor::resize(img, maxW, std::max(1, newH));
        }

        // Save
        QString baseName = QFileInfo(fname).completeBaseName();
        QString destPath = outDir.absoluteFilePath(baseName + "." + ext);

        // TGA: use CImg fallback (Qt has no TGA writer)
        // All others: try Qt (handles PNG, JPG, TIFF, BMP, WebP if plugin present)
        bool saved;
        if (ext == "tga") {
            QString saveErr;
            saved = FileManager::saveImage(img, destPath, &saveErr);
        } else {
            QImage qimg = ImageConvert::toQImage(img);
            saved = qimg.save(destPath, ext.toUpper().toLatin1().constData(), quality);
        }
        if (!saved) ++failed;

        ++processed;
        m_progress->setValue(processed);
        QApplication::processEvents();
    }

    setRunning(false);

    if (m_cancelFlag.load()) {
        m_statusLabel->setText(QString("Cancelled after %1 files.").arg(processed));
    } else {
        m_statusLabel->setText(
            QString("Done: %1 processed, %2 failed.").arg(processed - failed).arg(failed));
    }
}
