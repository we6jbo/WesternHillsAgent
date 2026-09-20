#pragma once

#include <QObject>
#include <QTcpServer>
#include <QString>

class QTcpSocket;

class PortManager : public QObject
{
    Q_OBJECT

public:
    explicit PortManager(QObject *parent = nullptr);
    ~PortManager() override;

    bool start();
    void stop();

    quint16 primaryPort() const;
    quint16 secondaryPort() const;
    bool isRunning() const;
    QString lastError() const;

    bool usingPersistentRegistry() const;
    QString registryStatus() const;
    QString registryDataDirectory() const;

signals:
    void connectionAccepted(QTcpSocket *socket, const QString &channel, const QString &peer);

private slots:
    void onPrimaryConnection();
    void onSecondaryConnection();

private:
    bool startUsingJeremiahPortGuard();
    bool startUsingTemporaryFallback();
    bool locateRegistry();
    bool openRegistry();
    void closeRegistry();
    bool registerProject();
    bool acquirePersistentEndpoint(QTcpServer &server,
                                   quint16 &selectedPort,
                                   const QString &purpose,
                                   const QString &channel);
    bool bindExistingAssignment(QTcpServer &server,
                                quint16 &selectedPort,
                                const QString &purpose,
                                const QString &channel);
    bool allocateNewAssignment(QTcpServer &server,
                               quint16 &selectedPort,
                               const QString &purpose,
                               const QString &channel);
    bool bindTemporaryPort(QTcpServer &server, quint16 &selectedPort, const QString &label);
    bool registryPortIsAvailable(quint16 port) const;
    bool updateAssignmentActive(quint16 port, const QString &purpose, const QString &channel);
    int ephemeralRangeStart() const;
    bool identityMatches(const QString &identityPath) const;
    bool isForbidden(quint16 port) const;
    void handOffPendingConnections(QTcpServer &server, const QString &channel);

    QTcpServer m_primaryServer;
    QTcpServer m_secondaryServer;
    quint16 m_primaryPort = 0;
    quint16 m_secondaryPort = 0;
    QString m_lastError;
    QString m_registryStatus;
    QString m_registryDataDir;
    QString m_registryDbPath;
    QString m_connectionName;
    bool m_usingPersistentRegistry = false;
};
