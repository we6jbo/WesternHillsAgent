#include "portmanager.h"

#include "agentidentity.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTcpSocket>
#include <QUuid>
#include <unistd.h>

namespace {
constexpr auto kPrimaryPurpose = "WesternHillsAgent primary communication endpoint";
constexpr auto kSecondaryPurpose = "WesternHillsAgent secondary agent coordination endpoint";
constexpr auto kApplicationId = "io.github.we6jbo.WesternHillsAgent";
constexpr auto kExpectedAuthorityApplicationId = "io.github.we6jbo.JeremiahPortGuard";

QString nowIso()
{
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}
}

PortManager::PortManager(QObject *parent)
    : QObject(parent),
      m_connectionName(QStringLiteral("WesternHillsAgentPortRegistry_%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    connect(&m_primaryServer, &QTcpServer::newConnection,
            this, &PortManager::onPrimaryConnection);
    connect(&m_secondaryServer, &QTcpServer::newConnection,
            this, &PortManager::onSecondaryConnection);
}

PortManager::~PortManager()
{
    stop();
    closeRegistry();
}

bool PortManager::start()
{
    stop();
    closeRegistry();
    m_lastError.clear();
    m_registryStatus.clear();
    m_registryDataDir.clear();
    m_registryDbPath.clear();
    m_usingPersistentRegistry = false;

    if (startUsingJeremiahPortGuard()) {
        m_usingPersistentRegistry = true;
        return true;
    }

    const QString registryFailure = m_lastError;
    stop();
    closeRegistry();
    m_lastError.clear();

    if (startUsingTemporaryFallback()) {
        m_registryStatus = QStringLiteral("Temporary fallback: JeremiahPortGuard persistent assignment unavailable. %1")
                               .arg(registryFailure);
        return true;
    }

    if (!registryFailure.isEmpty()) {
        m_lastError = QStringLiteral("Persistent registry failed (%1). Temporary fallback also failed (%2)")
                          .arg(registryFailure, m_lastError);
    }
    return false;
}

void PortManager::stop()
{
    if (m_usingPersistentRegistry && QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "UPDATE port_assignments SET assignment_status='MANAGED_ASSIGNED', is_listening=0, current_pid=NULL, "
                "actual_address=NULL, actual_port=NULL, last_verified_at=?, updated_at=? "
                "WHERE project_id=? AND protocol='TCP' AND bind_address='127.0.0.1' AND port IN (?,?)"));
            const QString timestamp = nowIso();
            q.addBindValue(timestamp);
            q.addBindValue(timestamp);
            q.addBindValue(AgentIdentity::projectId());
            q.addBindValue(m_primaryPort);
            q.addBindValue(m_secondaryPort);
            q.exec();
        }
    }

    m_primaryServer.close();
    m_secondaryServer.close();
    m_primaryPort = 0;
    m_secondaryPort = 0;
}

quint16 PortManager::primaryPort() const { return m_primaryPort; }
quint16 PortManager::secondaryPort() const { return m_secondaryPort; }
bool PortManager::isRunning() const { return m_primaryServer.isListening() && m_secondaryServer.isListening(); }
QString PortManager::lastError() const { return m_lastError; }
bool PortManager::usingPersistentRegistry() const { return m_usingPersistentRegistry; }
QString PortManager::registryStatus() const { return m_registryStatus; }
QString PortManager::registryDataDirectory() const { return m_registryDataDir; }

bool PortManager::startUsingJeremiahPortGuard()
{
    if (!locateRegistry())
        return false;
    if (!openRegistry())
        return false;
    if (!registerProject())
        return false;

    // From this point forward, stop() can safely return any bound persistent
    // assignments to MANAGED_ASSIGNED if a later startup step fails.
    m_usingPersistentRegistry = true;

    if (!acquirePersistentEndpoint(m_primaryServer, m_primaryPort,
                                   QString::fromLatin1(kPrimaryPurpose),
                                   QStringLiteral("primary"))) {
        return false;
    }

    if (!acquirePersistentEndpoint(m_secondaryServer, m_secondaryPort,
                                   QString::fromLatin1(kSecondaryPurpose),
                                   QStringLiteral("secondary"))) {
        m_primaryServer.close();
        m_primaryPort = 0;
        return false;
    }

    if (m_primaryPort == m_secondaryPort) {
        m_lastError = QStringLiteral("Registry returned the same TCP port for both communication endpoints.");
        stop();
        return false;
    }

    m_registryStatus = QStringLiteral("Persistent assignments managed through JeremiahPortGuard registry");
    return true;
}

bool PortManager::startUsingTemporaryFallback()
{
    if (!bindTemporaryPort(m_primaryServer, m_primaryPort, QStringLiteral("primary")))
        return false;

    if (!bindTemporaryPort(m_secondaryServer, m_secondaryPort, QStringLiteral("secondary"))) {
        m_primaryServer.close();
        m_primaryPort = 0;
        return false;
    }

    if (m_primaryPort == m_secondaryPort) {
        m_lastError = QStringLiteral("The operating system assigned the same fallback port twice unexpectedly.");
        stop();
        return false;
    }

    return true;
}

bool PortManager::locateRegistry()
{
    const QStringList candidates = {
        QStringLiteral("/var/lib/JeremiahPortGuard"),
        QStringLiteral("/var/lib/io.github.we6jbo.JeremiahPortGuard"),
        QStringLiteral("/usr/local/var/lib/JeremiahPortGuard"),
        QStringLiteral("/opt/JeremiahPortGuard/var/lib")
    };

    for (const QString &candidate : candidates) {
        const QString identityPath = candidate + QStringLiteral("/identity.json");
        const QString dbPath = candidate + QStringLiteral("/ports.db");
        if (QDir(candidate).exists() && QFileInfo::exists(dbPath) && identityMatches(identityPath)) {
            m_registryDataDir = candidate;
            m_registryDbPath = dbPath;
            return true;
        }
    }

    m_lastError = QStringLiteral("Could not locate an initialized JeremiahPortGuard ports.db with matching identity.json.");
    return false;
}

bool PortManager::openRegistry()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_registryDbPath);
    if (!db.open()) {
        m_lastError = QStringLiteral("Could not open JeremiahPortGuard registry: %1").arg(db.lastError().text());
        return false;
    }

    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    q.exec(QStringLiteral("PRAGMA busy_timeout=5000"));

    if (!db.tables().contains(QStringLiteral("projects")) ||
        !db.tables().contains(QStringLiteral("port_assignments"))) {
        m_lastError = QStringLiteral("JeremiahPortGuard registry exists but required tables are missing.");
        return false;
    }

    return true;
}

void PortManager::closeRegistry()
{
    if (!QSqlDatabase::contains(m_connectionName))
        return;

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool PortManager::registerProject()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        m_lastError = QStringLiteral("JeremiahPortGuard database is not open.");
        return false;
    }

    const QString timestamp = nowIso();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO projects(project_id, application_id, display_name, project_root, executable_path, version, owner_uid, owner_gid, tg_aka_identifiers, created_at, updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(project_id) DO UPDATE SET "
        "application_id=excluded.application_id, display_name=excluded.display_name, project_root=excluded.project_root, "
        "executable_path=excluded.executable_path, version=excluded.version, owner_uid=excluded.owner_uid, owner_gid=excluded.owner_gid, "
        "tg_aka_identifiers=excluded.tg_aka_identifiers, updated_at=excluded.updated_at"));
    q.addBindValue(AgentIdentity::projectId());
    q.addBindValue(QString::fromLatin1(kApplicationId));
    q.addBindValue(AgentIdentity::displayName());
    q.addBindValue(QStringLiteral("/home/we6jbo/Projects/WesternHillsAgent"));
    q.addBindValue(QCoreApplication::applicationFilePath());
    q.addBindValue(QCoreApplication::applicationVersion());
    q.addBindValue(static_cast<qlonglong>(getuid()));
    q.addBindValue(static_cast<qlonglong>(getgid()));
    q.addBindValue(AgentIdentity::provenanceCodes().join(QStringLiteral(",")));
    q.addBindValue(timestamp);
    q.addBindValue(timestamp);

    if (!q.exec()) {
        m_lastError = QStringLiteral("Could not register WesternHillsAgent in JeremiahPortGuard: %1")
                          .arg(q.lastError().text());
        return false;
    }
    return true;
}

bool PortManager::acquirePersistentEndpoint(QTcpServer &server,
                                            quint16 &selectedPort,
                                            const QString &purpose,
                                            const QString &channel)
{
    if (bindExistingAssignment(server, selectedPort, purpose, channel))
        return true;

    // An existing assignment that is unavailable is a conflict, not permission to silently move it.
    // Only allocate a new persistent assignment when no assignment exists for this purpose.
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery check(db);
    check.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM port_assignments WHERE project_id=? AND protocol='TCP' AND bind_address='127.0.0.1' AND purpose=? AND persistent_assignment=1"));
    check.addBindValue(AgentIdentity::projectId());
    check.addBindValue(purpose);
    if (!check.exec() || !check.next()) {
        m_lastError = QStringLiteral("Could not inspect existing %1 assignment: %2")
                          .arg(channel, check.lastError().text());
        return false;
    }
    if (check.value(0).toInt() > 0) {
        if (m_lastError.isEmpty())
            m_lastError = QStringLiteral("The persistent %1 port assignment exists but cannot currently be bound.").arg(channel);
        return false;
    }

    return allocateNewAssignment(server, selectedPort, purpose, channel);
}

bool PortManager::bindExistingAssignment(QTcpServer &server,
                                         quint16 &selectedPort,
                                         const QString &purpose,
                                         const QString &channel)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT port FROM port_assignments "
        "WHERE project_id=? AND protocol='TCP' AND bind_address='127.0.0.1' AND purpose=? AND persistent_assignment=1 "
        "ORDER BY id LIMIT 1"));
    q.addBindValue(AgentIdentity::projectId());
    q.addBindValue(purpose);

    if (!q.exec()) {
        m_lastError = QStringLiteral("Could not read persistent %1 assignment: %2").arg(channel, q.lastError().text());
        return false;
    }
    if (!q.next())
        return false;

    const int value = q.value(0).toInt();
    if (value < 1 || value > 65535 || isForbidden(static_cast<quint16>(value))) {
        m_lastError = QStringLiteral("Persistent %1 assignment is invalid or prohibited: %2").arg(channel).arg(value);
        return false;
    }

    const quint16 candidate = static_cast<quint16>(value);
    if (!server.listen(QHostAddress::LocalHost, candidate)) {
        QSqlQuery conflict(db);
        conflict.prepare(QStringLiteral(
            "UPDATE port_assignments SET assignment_status='CONFLICT', is_listening=0, last_conflict_at=?, last_error=?, updated_at=? "
            "WHERE project_id=? AND protocol='TCP' AND bind_address='127.0.0.1' AND port=?"));
        conflict.addBindValue(nowIso());
        conflict.addBindValue(server.errorString());
        conflict.addBindValue(nowIso());
        conflict.addBindValue(AgentIdentity::projectId());
        conflict.addBindValue(candidate);
        conflict.exec();

        m_lastError = QStringLiteral("Persistent %1 port %2 is unavailable: %3")
                          .arg(channel).arg(candidate).arg(server.errorString());
        return false;
    }

    selectedPort = candidate;
    return updateAssignmentActive(candidate, purpose, channel);
}

bool PortManager::allocateNewAssignment(QTcpServer &server,
                                        quint16 &selectedPort,
                                        const QString &purpose,
                                        const QString &channel)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    const int ephemeralStart = ephemeralRangeStart();
    const int firstCandidate = qMin(65535, ephemeralStart - 1);

    for (int candidate = firstCandidate; candidate >= 1024; --candidate) {
        const quint16 port = static_cast<quint16>(candidate);
        if (isForbidden(port) || !registryPortIsAvailable(port))
            continue;

        if (!server.listen(QHostAddress::LocalHost, port))
            continue;

        if (!db.transaction()) {
            server.close();
            m_lastError = QStringLiteral("Could not start registry transaction for %1 assignment: %2")
                              .arg(channel, db.lastError().text());
            return false;
        }

        QSqlQuery q(db);
        const QString timestamp = nowIso();
        q.prepare(QStringLiteral(
            "INSERT INTO port_assignments(project_id, protocol, bind_address, port, purpose, assignment_status, persistent_assignment, "
            "assigned_at, last_verified_at, current_pid, is_listening, actual_address, actual_port, authorization_method, created_at, updated_at) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
        q.addBindValue(AgentIdentity::projectId());
        q.addBindValue(QStringLiteral("TCP"));
        q.addBindValue(QStringLiteral("127.0.0.1"));
        q.addBindValue(port);
        q.addBindValue(purpose);
        q.addBindValue(QStringLiteral("MANAGED_ACTIVE"));
        q.addBindValue(1);
        q.addBindValue(timestamp);
        q.addBindValue(timestamp);
        q.addBindValue(QCoreApplication::applicationPid());
        q.addBindValue(1);
        q.addBindValue(QStringLiteral("127.0.0.1"));
        q.addBindValue(port);
        q.addBindValue(QStringLiteral("JeremiahPortGuard_registry_v2"));
        q.addBindValue(timestamp);
        q.addBindValue(timestamp);

        if (q.exec() && db.commit()) {
            selectedPort = port;
            return true;
        }

        db.rollback();
        server.close();
        // A concurrent allocator may have claimed the candidate. Continue scanning.
    }

    m_lastError = QStringLiteral("JeremiahPortGuard could not allocate a free persistent %1 TCP port below the OS ephemeral range.")
                      .arg(channel);
    return false;
}

bool PortManager::bindTemporaryPort(QTcpServer &server, quint16 &selectedPort, const QString &label)
{
    for (int attempt = 0; attempt < 32; ++attempt) {
        if (!server.listen(QHostAddress::LocalHost, 0)) {
            m_lastError = QStringLiteral("Could not bind fallback %1 listener: %2")
                              .arg(label, server.errorString());
            return false;
        }

        const quint16 candidate = server.serverPort();
        if (!isForbidden(candidate)) {
            selectedPort = candidate;
            return true;
        }
        server.close();
    }

    m_lastError = QStringLiteral("Could not obtain a permitted temporary %1 TCP port after repeated attempts.").arg(label);
    return false;
}

bool PortManager::registryPortIsAvailable(quint16 port) const
{
    if (isForbidden(port))
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM port_assignments "
        "WHERE protocol='TCP' AND port=? AND assignment_status NOT IN ('RELEASED','RETIRED')"));
    q.addBindValue(port);
    if (!q.exec() || !q.next())
        return false;
    return q.value(0).toInt() == 0;
}

bool PortManager::updateAssignmentActive(quint16 port, const QString &purpose, const QString &channel)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    const QString timestamp = nowIso();
    q.prepare(QStringLiteral(
        "UPDATE port_assignments SET assignment_status='MANAGED_ACTIVE', purpose=?, last_verified_at=?, current_pid=?, "
        "is_listening=1, actual_address='127.0.0.1', actual_port=?, last_error=NULL, updated_at=? "
        "WHERE project_id=? AND protocol='TCP' AND bind_address='127.0.0.1' AND port=?"));
    q.addBindValue(purpose);
    q.addBindValue(timestamp);
    q.addBindValue(QCoreApplication::applicationPid());
    q.addBindValue(port);
    q.addBindValue(timestamp);
    q.addBindValue(AgentIdentity::projectId());
    q.addBindValue(port);
    if (!q.exec()) {
        m_lastError = QStringLiteral("Bound %1 port %2 but could not update registry state: %3")
                          .arg(channel).arg(port).arg(q.lastError().text());
        if (channel == QStringLiteral("primary"))
            m_primaryServer.close();
        else if (channel == QStringLiteral("secondary"))
            m_secondaryServer.close();
        return false;
    }
    return true;
}

int PortManager::ephemeralRangeStart() const
{
    QFile f(QStringLiteral("/proc/sys/net/ipv4/ip_local_port_range"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QList<QByteArray> parts = f.readAll().simplified().split(' ');
        if (!parts.isEmpty()) {
            bool ok = false;
            const int value = parts.first().toInt(&ok);
            if (ok && value > 1024 && value <= 65535)
                return value;
        }
    }

    // Linux commonly starts the ephemeral range at 32768. This fallback is used only
    // when the kernel setting cannot be read; every candidate is still checked against
    // both the registry and the kernel before it is reserved.
    return 32768;
}

bool PortManager::identityMatches(const QString &identityPath) const
{
    QFile f(identityPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    return doc.object().value(QStringLiteral("application_id")).toString() ==
           QString::fromLatin1(kExpectedAuthorityApplicationId);
}

bool PortManager::isForbidden(quint16 port) const
{
    return port == 0 || port == 23458 || port == 23459 || port == 45454;
}

void PortManager::onPrimaryConnection()
{
    handOffPendingConnections(m_primaryServer, QStringLiteral("primary"));
}

void PortManager::onSecondaryConnection()
{
    handOffPendingConnections(m_secondaryServer, QStringLiteral("secondary"));
}

void PortManager::handOffPendingConnections(QTcpServer &server, const QString &channel)
{
    while (server.hasPendingConnections()) {
        QTcpSocket *socket = server.nextPendingConnection();
        if (!socket)
            continue;

        const QString peer = QStringLiteral("%1:%2")
                                 .arg(socket->peerAddress().toString())
                                 .arg(socket->peerPort());

        // PortManager owns port allocation/listening only. AgentProtocol owns framing,
        // validation and responses after the connection is accepted.
        emit connectionAccepted(socket, channel, peer);
    }
}
