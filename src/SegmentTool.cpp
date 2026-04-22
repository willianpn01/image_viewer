#include "SegmentTool.hpp"
#include "Logger.hpp"
#include <queue>
#include <algorithm>
#include <cmath>
#include <QElapsedTimer>

std::vector<bool> SegmentTool::regionGrow(
    const CImgU8& img, int seedX, int seedY, int threshold)
{
    QElapsedTimer timer;
    timer.start();

    const int W = img.width(), H = img.height(), C = img.spectrum();
    std::vector<bool> mask(W * H, false);

    if (seedX < 0 || seedX >= W || seedY < 0 || seedY >= H) {
        LOG_WARNING("SegmentTool",
            QString("Seed point (%1,%2) out of bounds (%3x%4)")
                .arg(seedX).arg(seedY).arg(W).arg(H));
        return mask;
    }

    // Record seed colour (up to 4 channels)
    float seedColor[4] = {};
    for (int c = 0; c < C && c < 4; c++)
        seedColor[c] = (float)img(seedX, seedY, 0, c);

    // Chebyshev distance to seed colour
    auto dist = [&](int x, int y) -> int {
        float d = 0.f;
        for (int c = 0; c < C && c < 4; c++)
            d = std::max(d, std::abs((float)img(x, y, 0, c) - seedColor[c]));
        return (int)d;
    };

    const int dx[] = { 0,  0,  1, -1 };
    const int dy[] = { 1, -1,  0,  0 };

    std::queue<std::pair<int,int>> q;
    mask[seedY * W + seedX] = true;
    q.push({seedX, seedY});
    int visited = 1;

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();
        for (int d = 0; d < 4; d++) {
            int nx = cx + dx[d], ny = cy + dy[d];
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
            if (mask[ny * W + nx]) continue;
            if (dist(nx, ny) <= threshold) {
                mask[ny * W + nx] = true;
                q.push({nx, ny});
                ++visited;
            }
        }
    }

    LOG_DEBUG("SegmentTool",
        QString("BFS from (%1,%2) threshold=%3: %4 px selected / %5 px total in %6 ms")
            .arg(seedX).arg(seedY).arg(threshold)
            .arg(visited).arg(W * H).arg(timer.elapsed()));

    return mask;
}

int SegmentTool::countSelected(const std::vector<bool>& mask) {
    return (int)std::count(mask.begin(), mask.end(), true);
}

QImage SegmentTool::makeOverlay(const std::vector<bool>& mask, int width, int height) {
    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    const QRgb selColor = qRgba(0, 120, 255, 120);
    for (int y = 0; y < height; y++) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < width; x++) {
            if ((int)(y * width + x) < (int)mask.size() && mask[y * width + x])
                line[x] = selColor;
        }
    }
    return img;
}
