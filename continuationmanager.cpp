#include "continuationmanager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

ContinuationManager::ContinuationManager(QObject *parent)
    : QObject(parent)
{
}

QString ContinuationManager::configurationPath() const { return m_path; }
QUrl ContinuationManager::configuredUrl() const { return m_url; }
QString ContinuationManager::lastError() const { return m_lastError; }

bool ContinuationManager::ensureConfiguration()
{
    m_lastError.clear();
    const QFileInfo info(m_path);
    QDir dir(info.absolutePath());
    if (!dir.exists()) {
        m_lastError = QStringLiteral("Directory does not exist: %1").arg(info.absolutePath());
        return false;
    }

    if (!QFileInfo::exists(m_path)) {
        QFile f(m_path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly | QIODevice::Text)) {
            m_lastError = QStringLiteral("Cannot create %1 as the current user: %2").arg(m_path, f.errorString());
            return false;
        }
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), 1);
        root.insert(QStringLiteral("url"), QStringLiteral("https://www.google.com/"));
        root.insert(QStringLiteral("browser"), QStringLiteral("google-chrome"));
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        f.close();
    }

    return load();
}

bool ContinuationManager::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QStringLiteral("Cannot read %1: %2").arg(m_path, f.errorString());
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("Invalid JSON in %1: %2").arg(m_path, error.errorString());
        return false;
    }

    const QUrl url(doc.object().value(QStringLiteral("url")).toString());
    if (!isSafeWebUrl(url)) {
        m_lastError = QStringLiteral("continuation.json must contain an http:// or https:// URL.");
        return false;
    }

    m_url = url;
    return true;
}

bool ContinuationManager::isSafeWebUrl(const QUrl &url) const
{
    if (!url.isValid() || url.host().isEmpty())
        return false;
    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
}

bool ContinuationManager::openConfiguredUrl()
{
    if (!load())
        return false;

    const QString chromePath = QStringLiteral("/opt/google/chrome/chrome");
    if (QFileInfo::exists(chromePath) && QFileInfo(chromePath).isExecutable()) {
        if (QProcess::startDetached(chromePath, {m_url.toString()}))
            return true;
    }

    for (const QString &name : {QStringLiteral("google-chrome-stable"), QStringLiteral("google-chrome")}) {
        const QString executable = QStandardPaths::findExecutable(name);
        if (!executable.isEmpty() && QProcess::startDetached(executable, {m_url.toString()}))
            return true;
    }

    if (QDesktopServices::openUrl(m_url))
        return true;

    m_lastError = QStringLiteral("Could not start Google Chrome or the desktop browser for %1").arg(m_url.toString());
    return false;
}
