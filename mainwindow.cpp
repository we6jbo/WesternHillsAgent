#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "agentidentity.h"

#include <QDateTime>
#include <QProcess>
#include <QStringList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      m_portManager(this),
      m_continuationManager(this),
      m_agentProtocol(&m_portManager, &m_continuationManager,
                      &m_moltbookConnector, &m_localAgentConnector, this)
{
    ui->setupUi(this);

    connect(ui->localTestButton, &QPushButton::clicked,
            this, &MainWindow::runLocalAgentTest);
    connect(ui->openContinuationButton, &QPushButton::clicked,
            this, &MainWindow::openContinuation);
    connect(&m_portManager, &PortManager::connectionAccepted,
            &m_agentProtocol, &AgentProtocol::acceptConnection);
    connect(&m_agentProtocol, &AgentProtocol::protocolEvent,
            this, &MainWindow::noteProtocolEvent);

    populateIdentity();
    initializeContinuation();
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
    ui->identityValue->setText(QStringLiteral("%1 (%2)").arg(AgentIdentity::displayName(), AgentIdentity::senderType()));
    ui->creatorValue->setText(AgentIdentity::creatorName());
    ui->descriptionValue->setText(AgentIdentity::description());
    ui->provenanceValue->setText(AgentIdentity::provenanceCodes().join(QStringLiteral(", ")));
    appendLog(QStringLiteral("Started as software identity WesternHillsAgent, not as a human user."));
}

void MainWindow::initializeContinuation()
{
    if (!m_continuationManager.ensureConfiguration()) {
        ui->continuationPathValue->setText(m_continuationManager.configurationPath());
        ui->continuationUrlValue->setText(QStringLiteral("ERROR"));
        appendLog(QStringLiteral("Continuation configuration unavailable: %1").arg(m_continuationManager.lastError()));
        return;
    }
    ui->continuationPathValue->setText(m_continuationManager.configurationPath());
    ui->continuationUrlValue->setText(m_continuationManager.configuredUrl().toString());
    appendLog(QStringLiteral("Continuation configuration ready at %1").arg(m_continuationManager.configurationPath()));
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

    ui->networkStatusValue->setText(QStringLiteral("Listening on 127.0.0.1; v3 safe NDJSON protocol active"));
    ui->primaryPortValue->setText(QString::number(m_portManager.primaryPort()));
    ui->secondaryPortValue->setText(QString::number(m_portManager.secondaryPort()));
    ui->registryModeValue->setText(m_portManager.registryStatus());
    ui->registryPathValue->setText(m_portManager.registryDataDirectory().isEmpty()
                                       ? QStringLiteral("none - temporary fallback")
                                       : m_portManager.registryDataDirectory());

    appendLog(QStringLiteral("Primary localhost TCP port: %1").arg(m_portManager.primaryPort()));
    appendLog(QStringLiteral("Secondary localhost TCP port: %1").arg(m_portManager.secondaryPort()));
    appendLog(m_portManager.registryStatus());
    appendLog(QStringLiteral("Version 3 accepts newline-delimited JSON informational requests; computer-control commands remain disabled."));
}

void MainWindow::runLocalAgentTest()
{
    const QString response = m_agentCore.respondToLocalTest(ui->localTestInput->text());
    ui->localTestOutput->setText(response);
    appendLog(QStringLiteral("Local AgentCore test executed."));
}

void MainWindow::openContinuation()
{
    if (m_continuationManager.openConfiguredUrl()) {
        ui->continuationUrlValue->setText(m_continuationManager.configuredUrl().toString());
        appendLog(QStringLiteral("Opened continuation URL as the current desktop user: %1")
                      .arg(m_continuationManager.configuredUrl().toString()));
    } else {
        appendLog(QStringLiteral("Could not open continuation URL: %1").arg(m_continuationManager.lastError()));
    }
}

void MainWindow::noteProtocolEvent(const QString &message)
{
    appendLog(message);
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

    if (!QProcess::startDetached(program, args))
        appendLog(QStringLiteral("tg-register-project was unavailable or could not be started; bundled provenance remains active."));
    else
        appendLog(QStringLiteral("Requested local TG project registration."));
}
