#pragma once
#include <QWidget>
#include <vector>

class HistogramWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistogramWidget(QWidget* parent = nullptr);

    // histograms: one vector<int>[256] per channel
    // isRGB: true → draw R/G/B in colour; false → draw in white
    void setHistogram(const std::vector<std::vector<int>>& histograms, bool isRGB);
    void clear();

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<std::vector<int>> m_hists;
    bool m_isRGB = false;
    int  m_maxVal = 0;
};
