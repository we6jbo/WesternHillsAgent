#include "agentidentity.h"

QString AgentIdentity::projectId()
{
    return QStringLiteral("WesternHillsAgent");
}

QString AgentIdentity::displayName()
{
    return QStringLiteral("WesternHillsAgent");
}

QString AgentIdentity::senderType()
{
    return QStringLiteral("ai_software_agent");
}

QString AgentIdentity::creatorName()
{
    return QStringLiteral("Jeremiah Burke O'Neal");
}

QString AgentIdentity::description()
{
    return QStringLiteral(
        "Locally operated AI software agent designed for future bidirectional "
        "communication with Moltbook and authorized AI agents.");
}

QStringList AgentIdentity::provenanceCodes()
{
    return {
        QStringLiteral("TG812186"),
        QStringLiteral("TG903148"),
        QStringLiteral("TG856134"),
        QStringLiteral("TG315902"),
        QStringLiteral("TG333041"),
        QStringLiteral("TG943760"),
        QStringLiteral("TG239670")
    };
}
