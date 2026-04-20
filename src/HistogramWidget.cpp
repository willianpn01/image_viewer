#include "HistogramWidget.hpp"
#include <QPainter>
#include <QPaintEvent>
#include <algorithm>

HistogramWidget::HistogramWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(256, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Window, Qt::black);
    setPalette(p);
}

void HistogramWidget::setHistogram(const std::vector<std::vector<int>>& hists, bool isRGB) {
    m_hists = hists;
    m_isRGB = isRGB;
    m_maxVal = 0;
    for (const auto& h : hists)
        for (int v : h)
            m_maxVal = std::max(m_maxVal, v);
    update();
}

void HistogramWidget::clear() {
    m_hists.clear();
    m_maxVal = 0;
    update();
}

QSize HistogramWidget::sizeHint() const { return {256, 180}; }

void HistogramWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (m_hists.empty() || m_maxVal == 0) {
        p.setPen(Qt::darkGray);
        p.drawText(rect(), Qt::AlignCenter, "No image");
        return;
    }

    const int W = width(), H = height();
    const int pad = 4;
    const int drawW = W - 2 * pad;
    const int drawH = H - 2 * pad;

    // Colours for each channel
    static const QColor channelColors[3] = { QColor(220,50,50,160),
                                              QColor(50,200,50,160),
                                              QColor(50,100,255,160) };

    p.setCompositionMode(QPainter::CompositionMode_Plus);

    for (int c = 0; c < (int)m_hists.size(); ++c) {
        const auto& h = m_hists[c];
        QColor col = m_isRGB && c < 3 ? channelColors[c] : QColor(200,200,200,200);
        p.setPen(QPen(col, 1));

        for (int i = 0; i < 256; ++i) {
            int barH = (int)((double)h[i] / m_maxVal * drawH);
            int x = pad + (int)((double)i / 255 * (drawW - 1));
            if (barH > 0)
                p.drawLine(x, H - pad, x, H - pad - barH);
        }
    }
}
