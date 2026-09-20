#pragma once

#include <QString>
#include <QStringList>

class AgentIdentity
{
public:
    static QString projectId();
    static QString displayName();
    static QString senderType();
    static QString creatorName();
    static QString description();
    static QStringList provenanceCodes();
};
