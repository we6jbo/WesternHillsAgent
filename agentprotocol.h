#pragma once

#include <QObject>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <QJsonObject>

class QTcpSocket;
class PortManager;
class ContinuationManager;
class MoltbookConnector;
class LocalAgentConnector;

class AgentProtocol : public QObject
{
    Q_OBJECT
public:
    AgentProtocol(PortManager *ports,
                  ContinuationManager *continuation,
                  MoltbookConnector *moltbook,
                  LocalAgentConnector *localAgent,
                  QObject *parent = nullptr);

public slots:
    void acceptConnection(QTcpSocket *socket, const QString &channel, const QString &peer);

signals:
    void protocolEvent(const QString &message);

private:
    void processReadyRead(QTcpSocket *socket);
    void processLine(QTcpSocket *socket, const QByteArray &line);
    void sendJson(QTcpSocket *socket, const QJsonObject &object);
    QJsonObject baseResponse(const QString &messageType, const QString &requestId) const;
    QJsonObject errorResponse(const QString &requestId, const QString &code, const QString &message) const;

    PortManager *m_ports;
    ContinuationManager *m_continuation;
    MoltbookConnector *m_moltbook;
    LocalAgentConnector *m_localAgent;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QTcpSocket *, QString> m_channels;
    QHash<QTcpSocket *, QString> m_peers;
};
