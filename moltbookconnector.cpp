#include "moltbookconnector.h"
QString MoltbookConnector::status() const { return QStringLiteral("not_connected"); }
QString MoltbookConnector::specificationState() const
{
    return QStringLiteral("Awaiting verified Moltbook network/API specifications; no undocumented API is assumed.");
}
