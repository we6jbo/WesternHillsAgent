#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "agentidentity.h"

#include <QDateTime>
#include <QProcess>
#include <QStringList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_portManager(this)
{
    ui->setupUi(this);

    connect(ui->localTestButton, &QPushButton::clicked,
            this, &MainWindow::runLocalAgentTest);
    connect(&m_portManager, &PortManager::connectionObserved,
            this, &MainWindow::noteConnection);

    populateIdentity();
    startListeners();
    attemptTgRegistration();
}

MainWindow::~MainWindow()
{
    m_portManager.stop();
    delete ui;
}

void MainWindow::populateIdentity()
{
    ui->identityValue->setText(
        QStringLiteral("%1 (%2)")
            .arg(AgentIdentity::displayName(), AgentIdentity::senderType()));
    ui->creatorValue->setText(AgentIdentity::creatorName());
    ui->descriptionValue->setText(AgentIdentity::description());
    ui->provenanceValue->setText(AgentIdentity::provenanceCodes().join(QStringLiteral(", ")));

    appendLog(QStringLiteral("Started as software identity WesternHillsAgent, not as a human user."));
}

void MainWindow::startListeners()
{
    if (!m_portManager.start()) {
        ui->networkStatusValue->setText(QStringLiteral("ERROR"));
        ui->registryModeValue->setText(QStringLiteral("Unavailable"));
        ui->registryPathValue->setText(QStringLiteral("not available"));
        ui->primaryPortValue->setText(QStringLiteral("not assigned"));
        ui->secondaryPortValue->setText(QStringLiteral("not assigned"));
        appendLog(QStringLiteral("Port startup failed: %1").arg(m_portManager.lastError()));
        return;
    }

    ui->networkStatusValue->setText(QStringLiteral("Listening on 127.0.0.1 only; unauthenticated data is rejected"));
    ui->primaryPortValue->setText(QString::number(m_portManager.primaryPort()));
    ui->secondaryPortValue->setText(QString::number(m_portManager.secondaryPort()));
    ui->registryModeValue->setText(m_portManager.registryStatus());
    ui->registryPathValue->setText(
        m_portManager.registryDataDirectory().isEmpty()
            ? QStringLiteral("none - temporary fallback")
            : m_portManager.registryDataDirectory());

    appendLog(QStringLiteral("Primary localhost TCP port: %1").arg(m_portManager.primaryPort()));
    appendLog(QStringLiteral("Secondary localhost TCP port: %1").arg(m_portManager.secondaryPort()));
    appendLog(m_portManager.registryStatus());

    if (m_portManager.usingPersistentRegistry()) {
        appendLog(QStringLiteral("Both endpoints are persistent JeremiahPortGuard assignments."));
    } else {
        appendLog(QStringLiteral("Persistent registry assignment was unavailable; ports are temporary for this run."));
    }

    appendLog(QStringLiteral("Version 2 still rejects inbound application data until authentication and message framing are implemented."));
}

void MainWindow::runLocalAgentTest()
{
    const QString response = m_agentCore.respondToLocalTest(ui->localTestInput->text());
    ui->localTestOutput->setText(response);
    appendLog(QStringLiteral("Local AgentCore test executed."));
}

void MainWindow::noteConnection(const QString &channel, const QString &peer)
{
    appendLog(QStringLiteral("Observed and rejected unauthenticated %1 connection from %2.")
                  .arg(channel, peer));
}

void MainWindow::appendLog(const QString &message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    ui->logBox->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void MainWindow::attemptTgRegistration()
{
    const QString program = QStringLiteral("tg-register-project");
    const QStringList args = {
        QStringLiteral("--project-id"), AgentIdentity::projectId(),
        QStringLiteral("--project-root"), QStringLiteral("/home/we6jbo/Projects/WesternHillsAgent"),
        QStringLiteral("--codes"), AgentIdentity::provenanceCodes().join(QStringLiteral(",")),
        QStringLiteral("--reference"), QStringLiteral("WesternHillsAgent bundled project provenance")
    };

    if (!QProcess::startDetached(program, args)) {
        appendLog(QStringLiteral("tg-register-project was unavailable or could not be started; bundled provenance remains active."));
    } else {
        appendLog(QStringLiteral("Requested local TG project registration."));
    }
}
