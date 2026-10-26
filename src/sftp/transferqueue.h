#pragma once

#include <QObject>
#include <QList>
#include <QString>

namespace qterm2 {

enum class TransferStatus {
    Queued,
    InProgress,
    Paused,
    Completed,
    Failed
};

struct TransferItem {
    QString id;
    QString localPath;
    QString remotePath;
    bool isDownload;
    TransferStatus status;
    qint64 bytesTransferred;
    qint64 totalBytes;
};

class TransferQueue : public QObject
{
    Q_OBJECT
public:
    explicit TransferQueue(QObject *parent = nullptr);

    void enqueueDownload(const QString &remote, const QString &local);
    void enqueueUpload(const QString &local, const QString &remote);
    void pause(const QString &id);
    void resume(const QString &id);
    void cancel(const QString &id);
    QList<TransferItem> items() const { return m_items; }

signals:
    void itemUpdated(const TransferItem &item);
    void queueChanged();

private:
    QList<TransferItem> m_items;
};

} // namespace qterm2
