#pragma once

#include "domain/session.h"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QString>

/**
 * Small widgets/helpers shared by the MainWindow translation units.
 */
namespace qterm2::uihelpers {

inline QIcon dotIcon(const QString &colorTag, int size = 12)
{
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Session::colorForTag(colorTag)));
    p.drawEllipse(QRectF(0, 0, size, size).adjusted(1, 1, -1, -1));
    p.end();
    return QIcon(pm);
}

} // namespace qterm2::uihelpers
