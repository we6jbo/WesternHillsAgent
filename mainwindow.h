#pragma once

#include <QMainWindow>
#include "agentcore.h"
#include "portmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void runLocalAgentTest();
    void noteConnection(const QString &channel, const QString &peer);

private:
    void populateIdentity();
    void startListeners();
    void appendLog(const QString &message);
    void attemptTgRegistration();

    Ui::MainWindow *ui;
    AgentCore m_agentCore;
    PortManager m_portManager;
};
