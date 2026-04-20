#include "FilterDialog.hpp"
#include "ImageProcessor.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <algorithm>

FilterDialog::FilterDialog(
    const QString& title,
    const QVector<Param>& params,
    const cimg_library::CImg<unsigned char>& original,
    std::function<cimg_library::CImg<unsigned char>(
        const cimg_library::CImg<unsigned char>&, const QVector<double>&)> processor,
    QWidget* parent)
    : QDialog(parent)
    , m_params(params)
    , m_original(original)
    , m_processor(processor)
{
    setWindowTitle(title);
    setMinimumWidth(480);

    // Build initial values
    for (const auto& p : params) m_values.append(p.defaultVal);

    auto* mainLayout = new QVBoxLayout(this);

    // ── Preview ──────────────────────────────────────────────────────────────
    m_previewLabel = new QLabel(this);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(320, 240);
    m_previewLabel->setFrameShape(QFrame::StyledPanel);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_previewLabel);

    // ── Parameters ───────────────────────────────────────────────────────────
    if (!params.isEmpty()) {
        auto* groupBox = new QGroupBox("Parameters", this);
        auto* grid = new QGridLayout(groupBox);

        for (int i = 0; i < params.size(); ++i) {
            const Param& p = params[i];

            auto* nameLabel = new QLabel(p.name + ":", this);
            auto* valLabel  = new QLabel(this);
            auto* slider    = new QSlider(Qt::Horizontal, this);

            slider->setRange(0, p.steps);
            // Map defaultVal to slider position
            int defaultPos = (int)((p.defaultVal - p.min) / (p.max - p.min) * p.steps);
            slider->setValue(defaultPos);

            // Initial value label
            if (p.isInt)
                valLabel->setText(QString::number((int)p.defaultVal) + p.suffix);
            else
                valLabel->setText(QString::number(p.defaultVal, 'f', 2) + p.suffix);
            valLabel->setMinimumWidth(50);

            grid->addWidget(nameLabel, i, 0);
            grid->addWidget(slider,    i, 1);
            grid->addWidget(valLabel,  i, 2);

            m_sliders.append(slider);
            m_valLabels.append(valLabel);

            // Capture index for lambda
            connect(slider, &QSlider::valueChanged, this, [this, i](int v) {
                onSliderChanged(i, v);
            });
        }
        mainLayout->addWidget(groupBox);
    }

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    updatePreview();
}

void FilterDialog::onSliderChanged(int idx, int sliderVal) {
    const Param& p = m_params[idx];
    double val = p.min + (double)sliderVal / p.steps * (p.max - p.min);
    if (p.isInt) val = std::round(val);
    m_values[idx] = val;

    if (p.isInt)
        m_valLabels[idx]->setText(QString::number((int)val) + p.suffix);
    else
        m_valLabels[idx]->setText(QString::number(val, 'f', 2) + p.suffix);

    updatePreview();
}

void FilterDialog::updatePreview() {
    if (m_original.is_empty()) return;

    // Downscale for fast preview (max 320×240)
    constexpr int PW = 320, PH = 240;
    double scale = std::min({ 1.0,
                              (double)PW / m_original.width(),
                              (double)PH / m_original.height() });

    int sw = std::max(1, (int)(m_original.width()  * scale));
    int sh = std::max(1, (int)(m_original.height() * scale));

    cimg_library::CImg<unsigned char> small =
        m_original.get_resize(sw, sh, -100, -100, 3);

    cimg_library::CImg<unsigned char> result = m_processor(small, m_values);

    QImage qi = ImageConvert::toQImage(result);
    m_previewLabel->setPixmap(QPixmap::fromImage(qi));
}
