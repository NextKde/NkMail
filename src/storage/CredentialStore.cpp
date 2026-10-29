#include "CredentialStore.h"
#include <qt6keychain/keychain.h>

namespace nkmail {
CredentialStore::CredentialStore(QObject *parent)
    : CredentialStore(QStringLiteral("org.nextkde.NkMail"), parent) {}
CredentialStore::CredentialStore(const QString &service, QObject *parent)
    : QObject(parent), m_service(service) {}

void CredentialStore::read(const QString &accountId)
{
    auto *job = new QKeychain::ReadPasswordJob(m_service, this);
    job->setKey(accountId);
    job->setInsecureFallback(false);
    connect(job, &QKeychain::Job::finished, this, [this, job, accountId] {
        if (job->error() != QKeychain::NoError)
            emit error(accountId, job->errorString());
        else
            emit passwordReady(accountId, job->textData());
    });
    job->start();
}
void CredentialStore::write(const QString &accountId, const QString &password)
{
    auto *job = new QKeychain::WritePasswordJob(m_service, this);
    job->setKey(accountId);
    job->setTextData(password);
    job->setInsecureFallback(false);
    connect(job, &QKeychain::Job::finished, this, [this, job, accountId] {
        if (job->error() != QKeychain::NoError)
            emit error(accountId, job->errorString());
        else
            emit passwordSaved(accountId);
    });
    job->start();
}
void CredentialStore::remove(const QString &accountId)
{
    auto *job = new QKeychain::DeletePasswordJob(m_service, this);
    job->setKey(accountId);
    job->setInsecureFallback(false);
    connect(job, &QKeychain::Job::finished, this, [this, job, accountId] {
        if (job->error() != QKeychain::NoError)
            emit error(accountId, job->errorString());
        else
            emit passwordDeleted(accountId);
    });
    job->start();
}
}
