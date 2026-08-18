#pragma once

#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtCore/QStringView>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtSvg/QSvgRenderer>

inline QIcon indexedWindowIcon(const QString &path, int index)
{
    QFile svg(path);
    if (!svg.open(QIODevice::ReadOnly))
        return QIcon(path);

    QByteArray svg_data = svg.readAll();
    svg_data.replace("{{I}}", QByteArray::number(index));

    QIcon icon;
    QSvgRenderer renderer(svg_data);
    for (int size : {16, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();

        icon.addPixmap(pixmap);
    }

    return icon;
}