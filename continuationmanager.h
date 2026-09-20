#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class ContinuationManager : public QObject
{
    Q_OBJECT
public:
    explicit ContinuationManager(QObject *parent = nullptr);

    bool ensureConfiguration();
    QString configurationPath() const;
    QUrl configuredUrl() const;
    QString lastError() const;
    bool openConfiguredUrl();

private:
    bool load();
    bool isSafeWebUrl(const QUrl &url) const;

    const QString m_path = QStringLiteral("/opt/web213/continuation.json");
    QUrl m_url;
    QString m_lastError;
};
