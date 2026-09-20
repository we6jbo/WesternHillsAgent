#include "localagentconnector.h"
QString LocalAgentConnector::status() const { return QStringLiteral("not_connected"); }
QString LocalAgentConnector::specificationState() const
{
    return QStringLiteral("Awaiting local Linux AI network specifications planned for 2026-09-20 before 2 PM.");
}
