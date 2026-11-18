#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QFutureWatcher>

namespace qterm2 {

enum class TransferStatus {
    Queued,
    InProgress,
    Completed,
    Failed,
    Canceled
};

struct TransferItem {
    QString id;
    QString localPath;
    QString remotePath;
    bool isDownload = true;
    TransferStatus status = TransferStatus::Queued;
    qint64 bytesTransferred = 0;
    qint64 totalBytes = 0;
    QString error;
};

struct TransferEndpoint
{
    QString host;
    int port = 22;
    QString user;
    QString keyFile;
};

class TransferQueue : public QObject
{
    Q_OBJECT
public:
    explicit TransferQueue(QObject *parent = nullptr);

    void setEndpoint(const TransferEndpoint &endpoint);
    void enqueueDownload(const QString &remote, const QString &local);
    void enqueueUpload(const QString &local, const QString &remote);
    void cancel(const QString &id);
    void retry(const QString &id);
    void clearFinished();
    QList<TransferItem> items() const { return m_items; }

signals:
    void itemUpdated(const TransferItem &item);
    void queueChanged();

private:
    struct JobResult { bool ok = false; QString error; qint64 bytes = 0; };
    void startNext();
    int indexOf(const QString &id) const;
    void updateItem(int index, const TransferItem &item);

    QList<TransferItem> m_items;
    TransferEndpoint m_endpoint;
    bool m_running = false;
    QString m_activeId;
    QFutureWatcher<JobResult> *m_watcher = nullptr;
};

} // namespace qterm2
