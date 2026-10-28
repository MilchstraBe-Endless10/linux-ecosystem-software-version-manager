#pragma once

#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QQuickImageProvider>
#include <QRegularExpression>
#include <QSize>
#include <QString>

// Qt 6 sketch for resolving *registered Freedesktop theme names* from Icon=.
// Install on an existing QQmlApplicationEngine:
//   engine.addImageProvider("appicons", new DesktopIconProvider);
// QML usage: source: "image://appicons/" + encodeURIComponent(model.iconName)
//
// This is deliberately not an arbitrary file-path loader. Absolute Icon= paths
// and AppStream/GIcon paths require a separate vetted native resolver.
class DesktopIconProvider final : public QQuickImageProvider {
public:
    DesktopIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override {
        static const QRegularExpression valid(
            QStringLiteral(R"(^[\p{L}\p{N}._+-]{1,160}$)"));
        const auto pixels = qBound(16, requestedSize.isValid()
                                         ? qMax(requestedSize.width(), requestedSize.height())
                                         : 48,
                                   128);
        const QSize target(pixels, pixels);
        const auto themedName = valid.match(id).hasMatch()
                                    ? id : QStringLiteral("application-x-executable");
        const auto fallback = QIcon::fromTheme(QStringLiteral("application-x-executable"));
        QIcon icon = QIcon::fromTheme(themedName, fallback);
        QImage image = icon.pixmap(target).toImage();
        if (image.isNull()) {
            image = QImage(target, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(QColor("#7586bb"), 2));
            painter.drawRoundedRect(image.rect().adjusted(4, 4, -5, -5), 6, 6);
            painter.drawText(image.rect(), Qt::AlignCenter, QStringLiteral("?"));
        }
        if (size)
            *size = image.size();
        return image;
    }
};
