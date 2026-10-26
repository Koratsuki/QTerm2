#pragma once

#include "ui/statusbar/statusbarcomponent.h"
#include <QLabel>
#include <QPainter>
#include <QTimer>

namespace qterm2 {

class UptimeComponent : public StatusBarComponent
{
    Q_OBJECT
public:
    explicit UptimeComponent(QObject *parent = nullptr);
    QString componentId() const override { return QStringLiteral("uptime"); }
    QString displayName() const override { return QStringLiteral("Uptime"); }
    QWidget *createWidget(QWidget *parent) override;

private slots:
    void updateUptime();

private:
    QLabel *m_label = nullptr;
    QTimer *m_timer = nullptr;
    qint64 m_bootTime = 0;
};

class CPUGraphComponent : public StatusBarComponent
{
    Q_OBJECT
public:
    explicit CPUGraphComponent(QObject *parent = nullptr);
    QString componentId() const override { return QStringLiteral("cpugraph"); }
    QString displayName() const override { return QStringLiteral("CPU Graph"); }
    QWidget *createWidget(QWidget *parent) override;

private slots:
    void updateCPU();

private:
    class GraphWidget : public QWidget
    {
    public:
        explicit GraphWidget(QWidget *parent = nullptr);
        void addValue(double value);
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:
        QVector<double> m_values;
    };
    
    GraphWidget *m_graph = nullptr;
    QTimer *m_timer = nullptr;
    QVector<qulonglong> m_lastCpuTotal;
    QVector<qulonglong> m_lastCpuIdle;
};

class MemoryGraphComponent : public StatusBarComponent
{
    Q_OBJECT
public:
    explicit MemoryGraphComponent(QObject *parent = nullptr);
    QString componentId() const override { return QStringLiteral("memgraph"); }
    QString displayName() const override { return QStringLiteral("Memory Graph"); }
    QWidget *createWidget(QWidget *parent) override;

private slots:
    void updateMemory();

private:
    class GraphWidget : public QWidget
    {
    public:
        explicit GraphWidget(QWidget *parent = nullptr);
        void addValue(double value);
    protected:
        void paintEvent(QPaintEvent *event) override;
    private:
        QVector<double> m_values;
    };
    
    GraphWidget *m_graph = nullptr;
    QTimer *m_timer = nullptr;
};

} // namespace qterm2
