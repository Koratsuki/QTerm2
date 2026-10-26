#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QMessageBox>
#include <QStyleFactory>

#include "app/applicationcontext.h"
#include "storage/settingsstore.h"
#include "ui/mainwindow.h"
#include "ui/theme.h"

namespace {

/** Small generated app icon: a dark rounded square with a ">_" prompt. */
QIcon makeAppIcon()
{
    QPixmap pixmap(128, 128);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient gradient(0, 0, 0, 128);
    gradient.setColorAt(0.0, QColor(0x2d, 0x38, 0x4a));
    gradient.setColorAt(1.0, QColor(0x16, 0x1c, 0x26));
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRectF(4, 4, 120, 120), 26, 26);

    QFont font;
    font.setFamilies({QStringLiteral("DejaVu Sans Mono"),
                      QStringLiteral("Monospace")});
    font.setPixelSize(64);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(0x7a, 0xa2, 0xf7));
    painter.drawText(QRectF(18, 20, 92, 88), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral(">"));
    painter.setPen(QColor(0x5c, 0xd6, 0x9a));
    painter.drawText(QRectF(48, 20, 70, 88), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("_"));

    painter.end();
    return QIcon(pixmap);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QTerm2"));
    QApplication::setApplicationDisplayName(QStringLiteral("QTerm2"));
    QApplication::setOrganizationName(QStringLiteral("QTerm2"));
    QApplication::setOrganizationDomain(QStringLiteral("qterm2.local"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setDesktopFileName(QStringLiteral("QTerm2"));

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    qterm2::ApplicationContext context;
    QString error;
    if (!context.initialize(&error)) {
        QMessageBox::critical(nullptr, QObject::tr("QTerm2"),
                              QObject::tr("No se pudo iniciar QTerm2:\n%1").arg(error));
        return 1;
    }

    const bool dark = context.settings()
                          ->value(QStringLiteral("ui/darkTheme"), true)
                          .toBool();
    Theme::setCurrentMode(dark ? Theme::Mode::Dark : Theme::Mode::Light);
    Theme::apply(app);

    app.setWindowIcon(makeAppIcon());

    MainWindow window(&context);
    window.show();

    return app.exec();
}
