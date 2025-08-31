#pragma once

#include <QObject>
#include <QDBusInterface>
#include <QString>
#include <QStringList>
#include <sys/types.h>
#include <functional>
#include <memory>

#define BEEKEEPER_ACTION_ID "org.beekeeper.privileged"

namespace beekeeper { namespace auth {

class PolkitManager : public QObject
{
    Q_OBJECT

public:
    explicit PolkitManager(QObject *parent = nullptr);
    ~PolkitManager();

    bool isAuthorized() const;
    bool requestAuthorization(const QString &message = QString());
    QString lastError() const { return m_lastError; }
    bool requestInitialAuthentication();
    bool checkAuthorization();
    bool isSessionEnabled() const;
    void setSessionEnabled(bool enabled);
    void clearSession();
    pid_t runPrivilegedHelper(const QString &helperPath, const QStringList &args);

    const std::string& getLastToken() const { return lastToken; }
    void setLastToken(const std::string token) { this->lastToken = token; }

signals:
    void authorizationChanged(bool authorized);
    void authenticationRequired(const QString &message);
    void sessionEnabledChanged(bool enabled);
    void sessionStarted();
    void sessionCleared();

private:
    bool performAuthentication(const QString &message);

    std::unique_ptr<QDBusInterface> m_polkitInterface;
    bool m_authorized;
    QString m_actionId;
    QString m_lastError;
    bool m_sessionEnabled;
    QString m_authenticationCookie;

    std::string lastToken;
};


typedef QMap<QString, QString> StringMap;

// Define our custom types in the global namespace
struct PolkitSubject {
    QString kind;
    QVariantMap details;
};

// Define a structure for the PolicyKit authorization result
// FIXED: Removed session_cookie field as it's not part of the PolicyKit response
struct CheckAuthorizationResult {
    bool authorized;
    bool challenge;
    StringMap details;
};

}} // namespace beekeeper::auth