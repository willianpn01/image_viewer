#include "ImageLabel.hpp"
#include <QMouseEvent>

ImageLabel::ImageLabel(QWidget* parent) : QLabel(parent) {
    setMouseTracking(true);
}

void ImageLabel::setTool(Tool t) {
    m_tool = t;
    if (t == Tool::Crop || t == Tool::MagicSelect || t == Tool::Annotate)
        setCursor(Qt::CrossCursor);
    else
        unsetCursor();
    if (m_rubberBand) m_rubberBand->hide();
}

void ImageLabel::setSelectionOverlay(const QImage& overlay) {
    m_selectionOverlay = overlay;
    update();
}

void ImageLabel::clearSelectionOverlay() {
    m_selectionOverlay = QImage();
    update();
}

void ImageLabel::setAnnotationOverlay(const QImage& overlay) {
    m_annotationOverlay = overlay;
    update();
}

void ImageLabel::clearAnnotationOverlay() {
    m_annotationOverlay = QImage();
    update();
}

// ── Press ─────────────────────────────────────────────────────────────────────

void ImageLabel::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_tool == Tool::Crop) {
        m_cropOrigin = e->pos();
        if (!m_rubberBand)
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        m_rubberBand->setGeometry(QRect(m_cropOrigin, QSize()));
        m_rubberBand->show();
        e->accept();
        return;
    }
    if (e->button() == Qt::LeftButton && m_tool == Tool::MagicSelect) {
        QPixmap pm = pixmap();
        if (!pm.isNull()) {
            int offX = (width()  - pm.width())  / 2;
            int offY = (height() - pm.height()) / 2;
            QPoint imgPt(
                (int)((e->pos().x() - offX) / m_zoom),
                (int)((e->pos().y() - offY) / m_zoom));
            emit pixelClicked(imgPt);
        }
        e->accept();
        return;
    }
    if (e->button() == Qt::LeftButton && m_tool == Tool::Annotate) {
        QPixmap pm = pixmap();
        if (!pm.isNull()) {
            int offX = (width()  - pm.width())  / 2;
            int offY = (height() - pm.height()) / 2;
            QPointF imgPt(
                (e->pos().x() - offX) / m_zoom,
                (e->pos().y() - offY) / m_zoom);
            emit annotatePress(imgPt);
        }
        e->accept();
        return;
    }
    e->ignore();
}

// ── Move ──────────────────────────────────────────────────────────────────────

void ImageLabel::mouseMoveEvent(QMouseEvent* e) {
    if (m_tool == Tool::Crop && m_rubberBand && (e->buttons() & Qt::LeftButton)) {
        m_rubberBand->setGeometry(QRect(m_cropOrigin, e->pos()).normalized());
        e->accept();
        return;
    }
    if (m_tool == Tool::Annotate && (e->buttons() & Qt::LeftButton)) {
        QPixmap pm = pixmap();
        if (!pm.isNull()) {
            int offX = (width()  - pm.width())  / 2;
            int offY = (height() - pm.height()) / 2;
            QPointF imgPt(
                (e->pos().x() - offX) / m_zoom,
                (e->pos().y() - offY) / m_zoom);
            emit annotateMove(imgPt);
        }
        e->accept();
        return;
    }
    e->ignore();
}

// ── Release ───────────────────────────────────────────────────────────────────

void ImageLabel::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && m_tool == Tool::Crop && m_rubberBand) {
        QRect displayRect = QRect(m_cropOrigin, e->pos()).normalized();
        m_rubberBand->hide();
        setTool(Tool::None);

        if (displayRect.width() > 4 && displayRect.height() > 4) {
            QPixmap pm = pixmap();
            if (!pm.isNull()) {
                int offX = (width()  - pm.width())  / 2;
                int offY = (height() - pm.height()) / 2;
                QRect imgCoords(
                    (int)((displayRect.x() - offX) / m_zoom),
                    (int)((displayRect.y() - offY) / m_zoom),
                    (int)(displayRect.width()  / m_zoom),
                    (int)(displayRect.height() / m_zoom));
                emit cropSelected(imgCoords);
            }
        }
        e->accept();
        return;
    }
    if (e->button() == Qt::LeftButton && m_tool == Tool::Annotate) {
        QPixmap pm = pixmap();
        if (!pm.isNull()) {
            int offX = (width()  - pm.width())  / 2;
            int offY = (height() - pm.height()) / 2;
            QPointF imgPt(
                (e->pos().x() - offX) / m_zoom,
                (e->pos().y() - offY) / m_zoom);
            emit annotateRelease(imgPt);
        }
        e->accept();
        return;
    }
    e->ignore();
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void ImageLabel::paintEvent(QPaintEvent* e) {
    QLabel::paintEvent(e);

    QPixmap pm = pixmap();
    if (pm.isNull()) return;

    int offX = (width()  - pm.width())  / 2;
    int offY = (height() - pm.height()) / 2;

    QPainter p(this);

    if (!m_selectionOverlay.isNull()) {
        QImage scaled = m_selectionOverlay.scaled(
            pm.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        p.drawImage(offX, offY, scaled);
    }

    if (!m_annotationOverlay.isNull()) {
        QImage scaled = m_annotationOverlay.scaled(
            pm.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
        p.drawImage(offX, offY, scaled);
    }
}
