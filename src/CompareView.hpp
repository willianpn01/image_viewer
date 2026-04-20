#pragma once
#include <QWidget>
#include <QSplitter>
#include <QScrollArea>
#include <QLabel>
#include <QScrollBar>

// Two synchronized scroll areas shown side by side.
// Left = original image, Right = edited image.
class CompareView : public QWidget {
    Q_OBJECT
public:
    explicit CompareView(QWidget* parent = nullptr);

    void setOriginal(const QPixmap& pm);
    void setEdited(const QPixmap& pm);

    // Resize both labels to match a common zoom level
    void setZoom(double zoom);

private slots:
    void syncFromLeft(int value);
    void syncFromRight(int value);

private:
    QSplitter*   m_splitter    = nullptr;

    QScrollArea* m_leftScroll  = nullptr;
    QScrollArea* m_rightScroll = nullptr;

    QWidget*     m_leftContainer  = nullptr;
    QWidget*     m_rightContainer = nullptr;

    QLabel*      m_leftImg    = nullptr;
    QLabel*      m_rightImg   = nullptr;

    QLabel*      m_leftBadge  = nullptr;  // "Original" overlay
    QLabel*      m_rightBadge = nullptr;  // "Editado"  overlay

    bool m_syncing = false;  // guard against recursive scroll signals

    QPixmap m_origPixmap;
    QPixmap m_editPixmap;
    double  m_zoom = 1.0;

    void setupBadge(QLabel* badge, const QString& text, QScrollArea* parent);
    void applyZoom();
};
