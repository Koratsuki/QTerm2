#include "ui/statusbar/components/systemcomponents.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QHBoxLayout>
#include <QRegularExpression>

namespace qterm2 {

UptimeComponent::UptimeComponent(QObject *parent)
    : StatusBarComponent(parent)
{
    // Read boot time from /proc/stat
    QFile file(QStringLiteral("/proc/stat"));
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(QStringLiteral("btime"))) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 2)
                    m_bootTime = parts[1].toLongLong();
                break;
            }
        }
        file.close();
    }
}

QWidget *UptimeComponent::createWidget(QWidget *parent)
{
    m_label = new QLabel(parent);
    m_label->setObjectName(QStringLiteral("StatusBarLabel"));
    m_timer = new QTimer(m_label);
    connect(m_timer, &QTimer::timeout, this, &UptimeComponent::updateUptime);
    m_timer->start(60000); // Update every minute
    updateUptime();
    return m_label;
}

void UptimeComponent::updateUptime()
{
    if (!m_label || m_bootTime == 0)
        return;
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 uptime = now - m_bootTime;
    qint64 days = uptime / 86400;
    qint64 hours = (uptime % 86400) / 3600;
    qint64 minutes = (uptime % 3600) / 60;
    
    if (days > 0)
        m_label->setText(QStringLiteral("up %1d %2h %3m").arg(days).arg(hours).arg(minutes));
    else if (hours > 0)
        m_label->setText(QStringLiteral("up %1h %2m").arg(hours).arg(minutes));
    else
        m_label->setText(QStringLiteral("up %1m").arg(minutes));
}

// CPU Graph
CPUGraphComponent::CPUGraphComponent(QObject *parent)
    : StatusBarComponent(parent)
{
}

QWidget *CPUGraphComponent::createWidget(QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 0, 4, 0);
    
    auto *label = new QLabel(QStringLiteral("CPU"), container);
    label->setObjectName(QStringLiteral("StatusBarLabel"));
    layout->addWidget(label);
    
    m_graph = new GraphWidget(container);
    m_graph->setFixedSize(60, 16);
    layout->addWidget(m_graph);
    
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &CPUGraphComponent::updateCPU);
    m_timer->start(1000);
    updateCPU();
    
    return container;
}

void CPUGraphComponent::updateCPU()
{
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    
    QTextStream in(&file);
    QString line = in.readLine();
    file.close();
    
    if (!line.startsWith(QStringLiteral("cpu ")))
        return;
    
    QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    if (parts.size() < 8)
        return;
    
    qulonglong user = parts[1].toULongLong();
    qulonglong nice = parts[2].toULongLong();
    qulonglong system = parts[3].toULongLong();
    qulonglong idle = parts[4].toULongLong();
    qulonglong iowait = parts[5].toULongLong();
    qulonglong irq = parts[6].toULongLong();
    qulonglong softirq = parts[7].toULongLong();
    
    qulonglong total = user + nice + system + idle + iowait + irq + softirq;
    qulonglong idleTotal = idle + iowait;
    
    if (!m_lastCpuTotal.isEmpty()) {
        qulonglong totalDiff = total - m_lastCpuTotal.last();
        qulonglong idleDiff = idleTotal - m_lastCpuIdle.last();
        if (totalDiff > 0) {
            double usage = 100.0 * (totalDiff - idleDiff) / totalDiff;
            if (m_graph)
                m_graph->addValue(usage);
        }
    }
    
    m_lastCpuTotal.clear();
    m_lastCpuIdle.clear();
    m_lastCpuTotal.append(total);
    m_lastCpuIdle.append(idleTotal);
}

CPUGraphComponent::GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void CPUGraphComponent::GraphWidget::addValue(double value)
{
    m_values.append(value);
    while (m_values.size() > 60)
        m_values.removeFirst();
    update();
}

void CPUGraphComponent::GraphWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setPen(QPen(Qt::green, 1));
    
    if (m_values.isEmpty())
        return;
    
    double step = width() / 60.0;
    for (int i = 0; i < m_values.size(); ++i) {
        double y = height() - (m_values[i] / 100.0) * height();
        if (i > 0) {
            double yPrev = height() - (m_values[i-1] / 100.0) * height();
            p.drawLine(QPointF((i-1) * step, yPrev), QPointF(i * step, y));
        }
    }
}

// Memory Graph
MemoryGraphComponent::MemoryGraphComponent(QObject *parent)
    : StatusBarComponent(parent)
{
}

QWidget *MemoryGraphComponent::createWidget(QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 0, 4, 0);
    
    auto *label = new QLabel(QStringLiteral("MEM"), container);
    label->setObjectName(QStringLiteral("StatusBarLabel"));
    layout->addWidget(label);
    
    m_graph = new GraphWidget(container);
    m_graph->setFixedSize(60, 16);
    layout->addWidget(m_graph);
    
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MemoryGraphComponent::updateMemory);
    m_timer->start(1000);
    updateMemory();
    
    return container;
}

void MemoryGraphComponent::updateMemory()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    
    QTextStream in(&file);
    qulonglong memTotal = 0, memFree = 0, memBuffers = 0, memCached = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith(QStringLiteral("MemTotal:")))
            memTotal = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)[1].toULongLong();
        else if (line.startsWith(QStringLiteral("MemFree:")))
            memFree = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)[1].toULongLong();
        else if (line.startsWith(QStringLiteral("Buffers:")))
            memBuffers = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)[1].toULongLong();
        else if (line.startsWith(QStringLiteral("Cached:")))
            memCached = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)[1].toULongLong();
    }
    file.close();
    
    if (memTotal > 0) {
        qulonglong used = memTotal - memFree - memBuffers - memCached;
        double usage = 100.0 * used / memTotal;
        if (m_graph)
            m_graph->addValue(usage);
    }
}

MemoryGraphComponent::GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void MemoryGraphComponent::GraphWidget::addValue(double value)
{
    m_values.append(value);
    while (m_values.size() > 60)
        m_values.removeFirst();
    update();
}

void MemoryGraphComponent::GraphWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setPen(QPen(Qt::cyan, 1));
    
    if (m_values.isEmpty())
        return;
    
    double step = width() / 60.0;
    for (int i = 0; i < m_values.size(); ++i) {
        double y = height() - (m_values[i] / 100.0) * height();
        if (i > 0) {
            double yPrev = height() - (m_values[i-1] / 100.0) * height();
            p.drawLine(QPointF((i-1) * step, yPrev), QPointF(i * step, y));
        }
    }
}

} // namespace qterm2
