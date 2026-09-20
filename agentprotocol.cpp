#include "agentprotocol.h"

#include "agentidentity.h"
#include "continuationmanager.h"
#include "localagentconnector.h"
#include "moltbookconnector.h"
#include "portmanager.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUuid>

namespace {
constexpr int kMaximumBufferedBytes = 64 * 1024;
constexpr int kMaximumMessageBytes = 16 * 1024;
}

AgentProtocol::AgentProtocol(PortManager *ports,
                             ContinuationManager *continuation,
                             MoltbookConnector *moltbook,
                             LocalAgentConnector *localAgent,
                             QObject *parent)
    : QObject(parent),
      m_ports(ports),
      m_continuation(continuation),
      m_moltbook(moltbook),
      m_localAgent(localAgent)
{
}

void AgentProtocol::acceptConnection(QTcpSocket *socket, const QString &channel, const QString &peer)
{
    if (!socket)
        return;

    m_buffers.insert(socket, QByteArray());
    m_channels.insert(socket, channel);
    m_peers.insert(socket, peer);

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { processReadyRead(socket); });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        emit protocolEvent(QStringLiteral("Connection closed: %1 channel from %2")
                               .arg(m_channels.value(socket), m_peers.value(socket)));
        m_buffers.remove(socket);
        m_channels.remove(socket);
        m_peers.remove(socket);
        socket->deleteLater();
    });

    emit protocolEvent(QStringLiteral("Accepted local %1 protocol connection from %2")
                           .arg(channel, peer));

    QJsonObject hello = baseResponse(QStringLiteral("hello"), QString());
    hello.insert(QStringLiteral("channel"), channel);
    hello.insert(QStringLiteral("authenticationRequiredForControl"), true);
    hello.insert(QStringLiteral("controlCommandsEnabled"), false);
    sendJson(socket, hello);
}

void AgentProtocol::processReadyRead(QTcpSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();
    if (buffer.size() > kMaximumBufferedBytes) {
        sendJson(socket, errorResponse(QString(), QStringLiteral("buffer_limit"), QStringLiteral("Input buffer limit exceeded.")));
        socket->disconnectFromHost();
        return;
    }

    while (true) {
        const qsizetype newline = buffer.indexOf('\n');
        if (newline < 0)
            break;

        QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.trimmed().isEmpty())
            continue;
        if (line.size() > kMaximumMessageBytes) {
            sendJson(socket, errorResponse(QString(), QStringLiteral("message_too_large"), QStringLiteral("Message exceeds 16 KiB.")));
            continue;
        }
        processLine(socket, line);
    }
}

void AgentProtocol::processLine(QTcpSocket *socket, const QByteArray &line)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        sendJson(socket, errorResponse(QString(), QStringLiteral("invalid_json"), QStringLiteral("Each message must be one JSON object followed by a newline.")));
        return;
    }

    const QJsonObject request = doc.object();
    const QString requestId = request.value(QStringLiteral("messageId")).toString();
    const int version = request.value(QStringLiteral("protocolVersion")).toInt(-1);
    const QString type = request.value(QStringLiteral("messageType")).toString();

    if (version != 1) {
        sendJson(socket, errorResponse(requestId, QStringLiteral("unsupported_protocol"), QStringLiteral("protocolVersion must be 1.")));
        return;
    }

    QJsonObject response;
    if (type == QStringLiteral("ping")) {
        response = baseResponse(QStringLiteral("pong"), requestId);
    } else if (type == QStringLiteral("identity")) {
        response = baseResponse(QStringLiteral("identity_response"), requestId);
        response.insert(QStringLiteral("projectId"), AgentIdentity::projectId());
        response.insert(QStringLiteral("displayName"), AgentIdentity::displayName());
        response.insert(QStringLiteral("senderType"), AgentIdentity::senderType());
        response.insert(QStringLiteral("creator"), AgentIdentity::creatorName());
    } else if (type == QStringLiteral("status_request")) {
        response = baseResponse(QStringLiteral("status_response"), requestId);
        response.insert(QStringLiteral("status"), QStringLiteral("running"));
        response.insert(QStringLiteral("primaryPort"), static_cast<int>(m_ports->primaryPort()));
        response.insert(QStringLiteral("secondaryPort"), static_cast<int>(m_ports->secondaryPort()));
        response.insert(QStringLiteral("persistentPortRegistry"), m_ports->usingPersistentRegistry());
        response.insert(QStringLiteral("moltbook"), m_moltbook->status());
        response.insert(QStringLiteral("localAgent"), m_localAgent->status());
    } else if (type == QStringLiteral("port_status")) {
        response = baseResponse(QStringLiteral("port_status_response"), requestId);
        response.insert(QStringLiteral("bindAddress"), QStringLiteral("127.0.0.1"));
        response.insert(QStringLiteral("primaryPort"), static_cast<int>(m_ports->primaryPort()));
        response.insert(QStringLiteral("secondaryPort"), static_cast<int>(m_ports->secondaryPort()));
        response.insert(QStringLiteral("registryStatus"), m_ports->registryStatus());
        response.insert(QStringLiteral("registryDirectory"), m_ports->registryDataDirectory());
    } else if (type == QStringLiteral("capabilities")) {
        response = baseResponse(QStringLiteral("capabilities_response"), requestId);
        QJsonArray safe;
        for (const char *name : {"ping", "identity", "status_request", "port_status", "capabilities", "network_specifications"})
            safe.append(QString::fromLatin1(name));
        response.insert(QStringLiteral("safeUnauthenticatedRequests"), safe);
        response.insert(QStringLiteral("remoteControl"), false);
        response.insert(QStringLiteral("arbitraryCommandExecution"), false);
        response.insert(QStringLiteral("messageFraming"), QStringLiteral("newline-delimited JSON"));
    } else if (type == QStringLiteral("network_specifications")) {
        response = baseResponse(QStringLiteral("network_specifications_response"), requestId);
        QJsonObject local;
        local.insert(QStringLiteral("bindAddress"), QStringLiteral("127.0.0.1"));
        local.insert(QStringLiteral("primaryPort"), static_cast<int>(m_ports->primaryPort()));
        local.insert(QStringLiteral("secondaryPort"), static_cast<int>(m_ports->secondaryPort()));
        local.insert(QStringLiteral("framing"), QStringLiteral("NDJSON"));
        local.insert(QStringLiteral("protocolVersion"), 1);
        response.insert(QStringLiteral("westernHillsAgent"), local);
        response.insert(QStringLiteral("moltbookSpecification"), m_moltbook->specificationState());
        response.insert(QStringLiteral("localAgentSpecification"), m_localAgent->specificationState());
    } else {
        response = errorResponse(requestId, QStringLiteral("unsupported_message_type"),
                                 QStringLiteral("This v3 endpoint accepts only non-control informational requests."));
    }

    emit protocolEvent(QStringLiteral("Handled %1 request on %2 channel from %3")
                           .arg(type.isEmpty() ? QStringLiteral("unknown") : type,
                                m_channels.value(socket), m_peers.value(socket)));
    sendJson(socket, response);
}

void AgentProtocol::sendJson(QTcpSocket *socket, const QJsonObject &object)
{
    if (!socket || socket->state() == QAbstractSocket::UnconnectedState)
        return;
    socket->write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    socket->write("\n");
    socket->flush();
}

QJsonObject AgentProtocol::baseResponse(const QString &messageType, const QString &requestId) const
{
    QJsonObject object;
    object.insert(QStringLiteral("protocolVersion"), 1);
    object.insert(QStringLiteral("messageId"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!requestId.isEmpty())
        object.insert(QStringLiteral("inReplyTo"), requestId);
    object.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("senderId"), AgentIdentity::projectId());
    object.insert(QStringLiteral("senderType"), AgentIdentity::senderType());
    object.insert(QStringLiteral("messageType"), messageType);
    object.insert(QStringLiteral("authenticated"), false);
    return object;
}

QJsonObject AgentProtocol::errorResponse(const QString &requestId, const QString &code, const QString &message) const
{
    QJsonObject object = baseResponse(QStringLiteral("error"), requestId);
    object.insert(QStringLiteral("errorCode"), code);
    object.insert(QStringLiteral("errorMessage"), message);
    return object;
}
