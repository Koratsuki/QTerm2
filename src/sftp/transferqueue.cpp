#include "sftp/transferqueue.h"
#include <QUuid>

namespace qterm2 {

TransferQueue::TransferQueue(QObject *parent)
    : QObject(parent)
{
}

void TransferQueue::enqueueDownload(const QString &remote, const QString &local)
{
    TransferItem item;
    item.id = QUuid::createUuid().toString();
    item.remotePath = remote;
    item.localPath = local;
    item.isDownload = true;
    item.status = TransferStatus::Queued;
    item.bytesTransferred = 0;
    item.totalBytes = 0;
    m_items.append(item);
    emit queueChanged();
}

void TransferQueue::enqueueUpload(const QString &local, const QString &remote)
{
    TransferItem item;
    item.id = QUuid::createUuid().toString();
    item.remotePath = remote;
    item.localPath = local;
    item.isDownload = false;
    item.status = TransferStatus::Queued;
    item.bytesTransferred = 0;
    item.totalBytes = 0;
    m_items.append(item);
    emit queueChanged();
}

void TransferQueue::pause(const QString &id)
{
    Q_UNUSED(id);
}

void TransferQueue::resume(const QString &id)
{
    Q_UNUSED(id);
}

void TransferQueue::cancel(const QString &id)
{
    Q_UNUSED(id);
}

} // namespace qterm2
