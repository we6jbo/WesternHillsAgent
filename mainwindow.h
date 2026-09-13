#pragma once

#include <QMainWindow>
#include "agentcore.h"
#include "portmanager.h"
#include "continuationmanager.h"
#include "moltbookconnector.h"
#include "localagentconnector.h"
#include "agentprotocol.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void runLocalAgentTest();
    void openContinuation();
    void noteProtocolEvent(const QString &message);

private:
    void populateIdentity();
    void startListeners();
    void initializeContinuation();
    void appendLog(const QString &message);
    void attemptTgRegistration();

    Ui::MainWindow *ui;
    AgentCore m_agentCore;
    PortManager m_portManager;
    ContinuationManager m_continuationManager;
    MoltbookConnector m_moltbookConnector;
    LocalAgentConnector m_localAgentConnector;
    AgentProtocol m_agentProtocol;
};
