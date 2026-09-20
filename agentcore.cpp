#include "agentcore.h"

QString AgentCore::respondToLocalTest(const QString &message) const
{
    const QString trimmed = message.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("WesternHillsAgent received an empty local test message.");
    }

    return QStringLiteral("WesternHillsAgent received: %1").arg(trimmed);
}
