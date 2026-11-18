#include "sftp/transferqueue.h"

#include "sftp/sftpservice.h"

#include <QFileInfo>
#include <QtConcurrent>
#include <QUuid>

namespace qterm2 {

TransferQueue::TransferQueue(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFutureWatcher<JobResult>(this))
{
    connect(m_watcher, &QFutureWatcher<JobResult>::finished, this, [this] {
        const QString id = m_activeId;
        m_activeId.clear();
        m_running = false;
        const int idx = indexOf(id);
        if (idx >= 0) {
            const JobResult r = m_watcher->result();
            TransferItem item = m_items[idx];
            if (item.status == TransferStatus::Canceled) {
                // keep canceled
            } else if (r.ok) {
                item.status = TransferStatus::Completed;
                item.bytesTransferred = r.bytes;
                item.totalBytes = r.bytes;
                item.error.clear();
            } else {
                item.status = TransferStatus::Failed;
                item.error = r.error;
            }
            m_items[idx] = item;
            emit itemUpdated(item);
            emit queueChanged();
        }
        startNext();
    });
}

void TransferQueue::setEndpoint(const TransferEndpoint &endpoint)
{
    m_endpoint = endpoint;
}

int TransferQueue::indexOf(const QString &id) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id)
            return i;
    }
    return -1;
}

void TransferQueue::enqueueDownload(const QString &remote, const QString &local)
{
    TransferItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.remotePath = remote;
    item.localPath = local;
    item.isDownload = true;
    item.status = TransferStatus::Queued;
    m_items.append(item);
    emit queueChanged();
    startNext();
}

void TransferQueue::enqueueUpload(const QString &local, const QString &remote)
{
    TransferItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.remotePath = remote;
    item.localPath = local;
    item.isDownload = false;
    item.status = TransferStatus::Queued;
    item.totalBytes = QFileInfo(local).size();
    m_items.append(item);
    emit queueChanged();
    startNext();
}

void TransferQueue::cancel(const QString &id)
{
    const int idx = indexOf(id);
    if (idx < 0)
        return;
    TransferItem &item = m_items[idx];
    if (item.status == TransferStatus::Queued) {
        item.status = TransferStatus::Canceled;
        emit itemUpdated(item);
        emit queueChanged();
    } else if (item.status == TransferStatus::InProgress && m_activeId == id) {
        // Best-effort: mark canceled; running job will finish but we keep state.
        item.status = TransferStatus::Canceled;
        item.error = tr("Cancelado (la operación en curso puede completar).");
        emit itemUpdated(item);
        emit queueChanged();
    }
}

void TransferQueue::retry(const QString &id)
{
    const int idx = indexOf(id);
    if (idx < 0)
        return;
    TransferItem &item = m_items[idx];
    if (item.status == TransferStatus::Failed || item.status == TransferStatus::Canceled) {
        item.status = TransferStatus::Queued;
        item.error.clear();
        item.bytesTransferred = 0;
        emit itemUpdated(item);
        emit queueChanged();
        startNext();
    }
}

void TransferQueue::clearFinished()
{
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const auto s = m_items[i].status;
        if (s == TransferStatus::Completed || s == TransferStatus::Failed || s == TransferStatus::Canceled)
            m_items.removeAt(i);
    }
    emit queueChanged();
}

void TransferQueue::startNext()
{
    if (m_running)
        return;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].status != TransferStatus::Queued)
            continue;
        TransferItem item = m_items[i];
        item.status = TransferStatus::InProgress;
        item.error.clear();
        m_items[i] = item;
        m_activeId = item.id;
        m_running = true;
        emit itemUpdated(item);
        emit queueChanged();

        const TransferEndpoint ep = m_endpoint;
        const bool download = item.isDownload;
        const QString remote = item.remotePath;
        const QString local = item.localPath;
        auto future = QtConcurrent::run([ep, download, remote, local]() -> JobResult {
            SftpService svc;
            JobResult r;
            if (!svc.connect(ep.host, ep.port, ep.user, ep.keyFile)) {
                r.ok = false;
                r.error = svc.lastError();
                return r;
            }
            bool ok = download ? svc.downloadFile(remote, local) : svc.uploadFile(local, remote);
            r.ok = ok;
            if (!ok)
                r.error = svc.lastError();
            else
                r.bytes = QFileInfo(download ? local : local).size();
            svc.disconnect();
            return r;
        });
        m_watcher->setFuture(future);
        return;
    }
}

} // namespace qterm2
