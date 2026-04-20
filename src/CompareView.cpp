#include "CompareView.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <cmath>

CompareView::CompareView(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(m_splitter);

    // ── Left panel ────────────────────────────────────────────────────────────
    m_leftScroll = new QScrollArea(m_splitter);
    m_leftScroll->setWidgetResizable(false);
    m_leftScroll->setAlignment(Qt::AlignCenter);
    m_leftScroll->setBackgroundRole(QPalette::Dark);

    m_leftImg = new QLabel;
    m_leftImg->setAlignment(Qt::AlignCenter);
    m_leftImg->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_leftScroll->setWidget(m_leftImg);

    // ── Right panel ───────────────────────────────────────────────────────────
    m_rightScroll = new QScrollArea(m_splitter);
    m_rightScroll->setWidgetResizable(false);
    m_rightScroll->setAlignment(Qt::AlignCenter);
    m_rightScroll->setBackgroundRole(QPalette::Dark);

    m_rightImg = new QLabel;
    m_rightImg->setAlignment(Qt::AlignCenter);
    m_rightImg->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_rightScroll->setWidget(m_rightImg);

    m_splitter->setSizes({500, 500});
    m_splitter->setChildrenCollapsible(false);

    // ── Overlaid badges ───────────────────────────────────────────────────────
    m_leftBadge  = new QLabel("Original", m_leftScroll->viewport());
    m_rightBadge = new QLabel("Editado",  m_rightScroll->viewport());
    setupBadge(m_leftBadge,  "Original", m_leftScroll);
    setupBadge(m_rightBadge, "Editado",  m_rightScroll);

    // ── Synchronised scrolling ─────────────────────────────────────────────────
    connect(m_leftScroll->horizontalScrollBar(),  &QScrollBar::valueChanged,
            this, &CompareView::syncFromLeft);
    connect(m_leftScroll->verticalScrollBar(),    &QScrollBar::valueChanged,
            this, &CompareView::syncFromLeft);
    connect(m_rightScroll->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &CompareView::syncFromRight);
    connect(m_rightScroll->verticalScrollBar(),   &QScrollBar::valueChanged,
            this, &CompareView::syncFromRight);
}

void CompareView::setupBadge(QLabel* badge, const QString& text, QScrollArea* /*parent*/) {
    badge->setText(text);
    badge->setStyleSheet(
        "QLabel { background: rgba(0,0,0,160); color: white; "
        "padding: 4px 8px; border-radius: 4px; font-weight: bold; }");
    badge->adjustSize();
    badge->move(8, 8);
    badge->raise();
    badge->show();
}

void CompareView::setOriginal(const QPixmap& pm) {
    m_origPixmap = pm;
    applyZoom();
}

void CompareView::setEdited(const QPixmap& pm) {
    m_editPixmap = pm;
    applyZoom();
}

void CompareView::setZoom(double zoom) {
    m_zoom = zoom;
    applyZoom();
}

void CompareView::applyZoom() {
    auto scale = [&](const QPixmap& src, QLabel* lbl) {
        if (src.isNull()) { lbl->clear(); return; }
        int w = std::max(1, (int)std::round(src.width()  * m_zoom));
        int h = std::max(1, (int)std::round(src.height() * m_zoom));
        lbl->setPixmap(src.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        lbl->resize(w, h);
    };
    scale(m_origPixmap, m_leftImg);
    scale(m_editPixmap, m_rightImg);
}

// ── Scroll synchronisation ────────────────────────────────────────────────────

void CompareView::syncFromLeft(int value) {
    if (m_syncing) return;
    m_syncing = true;
    QScrollBar* src = qobject_cast<QScrollBar*>(sender());
    // Determine if horizontal or vertical
    if (src == m_leftScroll->horizontalScrollBar())
        m_rightScroll->horizontalScrollBar()->setValue(value);
    else
        m_rightScroll->verticalScrollBar()->setValue(value);
    m_syncing = false;
}

void CompareView::syncFromRight(int value) {
    if (m_syncing) return;
    m_syncing = true;
    QScrollBar* src = qobject_cast<QScrollBar*>(sender());
    if (src == m_rightScroll->horizontalScrollBar())
        m_leftScroll->horizontalScrollBar()->setValue(value);
    else
        m_leftScroll->verticalScrollBar()->setValue(value);
    m_syncing = false;
}
