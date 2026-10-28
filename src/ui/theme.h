#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

class QApplication;

namespace Theme {

enum class Mode { Dark, Light };

struct TerminalColors
{
    QColor background;
    QColor foreground;
    QColor cursor;
    QColor selection;
    QColor palette[16];

    static TerminalColors dark();
    static TerminalColors light();
    static TerminalColors whiteOnBlack();
    static TerminalColors greenOnBlack();
    static TerminalColors forMode(Mode mode);
};

Mode currentMode();
void setCurrentMode(Mode mode);

void apply(QApplication &app);
QString styleSheet(Mode mode);
QPalette applicationPalette(Mode mode);

} // namespace Theme
