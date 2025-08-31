#include "beekeeper/debug.hpp"
#include "beekeeper/qt-debug.hpp"
#include "polkit.hpp"
#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include <QWidget>
#include <QProcess>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

using namespace beekeeper::auth;

Q_DECLARE_METATYPE(PolkitSubject);
Q_DECLARE_METATYPE(StringMap);
Q_DECLARE_METATYPE(CheckAuthorizationResult);

// Implement operators in the global namespace
QDBusArgument &operator<<(QDBusArgument &argument, const PolkitSubject &subject) {
    argument.beginStructure();
    argument << subject.kind;
    argument << subject.details;
    argument.endStructure();
    return argument;
}

const
QDBusArgument &operator>>(const QDBusArgument &argument, PolkitSubject &subject) {
    argument.beginStructure();
    argument >> subject.kind;
    argument >> subject.details;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const StringMap &map) {
    argument.beginMap(QMetaType::QString, QMetaType::QString);
    QMapIterator<QString, QString> i(map);
    while (i.hasNext()) {
        i.next();
        argument.beginMapEntry();
        argument << i.key() << i.value();
        argument.endMapEntry();
    }
    argument.endMap();
    return argument;
}

const
QDBusArgument &operator>>(const QDBusArgument &argument, StringMap &map) {
    argument.beginMap();
    map.clear();
    
    while (!argument.atEnd()) {
        QString key;
        QString value;
        argument.beginMapEntry();
        argument >> key;
        argument >> value;
        argument.endMapEntry();
        map.insert(key, value);
    }
    
    argument.endMap();
    return argument;
}

// Operators for CheckAuthorizationResult
QDBusArgument &operator<<(QDBusArgument &argument, const CheckAuthorizationResult &result) {
    argument.beginStructure();
    argument << result.authorized << result.challenge << result.details;
    argument.endStructure();
    return argument;
}

const
QDBusArgument &operator>>(const QDBusArgument &argument, CheckAuthorizationResult &result) {
    argument.beginStructure();
    argument >> result.authorized >> result.challenge >> result.details;
    argument.endStructure();
    return argument;
}

namespace {
    void registerMetaTypes() {
        static bool registered = false;
        if (!registered) {
            // Register with explicit type names
            qDBusRegisterMetaType<PolkitSubject>();
            qDBusRegisterMetaType<StringMap>();
            qDBusRegisterMetaType<CheckAuthorizationResult>();
            qRegisterMetaType<PolkitSubject>();
            qRegisterMetaType<StringMap>();
            qRegisterMetaType<CheckAuthorizationResult>();
            registered = true;
        }
    }

    // Function to get actual process start time
    quint64 getProcessStartTime() {
        QFile file(QString("/proc/%1/stat").arg(getpid()));
        if (!file.open(QIODevice::ReadOnly)) {
            return 0;
        }
        
        QTextStream in(&file);
        QStringList tokens = in.readAll().split(' ');
        if (tokens.size() < 22) {
            return 0;
        }
        
        // Start time is the 22nd token in /proc/pid/stat
        return tokens[21].toULongLong();
    }

    // Function to find the helper executable
    QString findHelperPath() {
        // Try various locations where the helper might be installed
        QStringList searchPaths = {
            QCoreApplication::applicationDirPath() + "/beekeeper-helper",
            "/usr/bin/beekeeper-helper",
            "/usr/local/bin/beekeeper-helper",
            "/opt/beekeeper/bin/beekeeper-helper"
        };
        
        for (const QString& path : searchPaths) {
            if (QFile::exists(path)) {
                DEBUG_LOG("Found helper at: ", path);
                return path;
            }
        }
        
        DEBUG_LOG("Helper not found in any standard location");
        return QString();
    }
}

namespace beekeeper { namespace auth {

PolkitManager::PolkitManager(QObject *parent)
    : QObject(parent)
    , m_authorized(false)
    , m_actionId(BEEKEEPER_ACTION_ID)
    , m_sessionEnabled(true)  // Default to session mode enabled
    , m_authenticationCookie("")
{
    registerMetaTypes();  // Ensure types are registered
    
    // Read settings for session management
    QSettings settings;
    m_sessionEnabled = settings.value("polkit/session_enabled", true).toBool();
    
    // Create D-Bus interface
    m_polkitInterface = std::make_unique<QDBusInterface>(
        "org.freedesktop.PolicyKit1",
        "/org/freedesktop/PolicyKit1/Authority",
        "org.freedesktop.PolicyKit1.Authority",
        QDBusConnection::systemBus()
    );

    if (!m_polkitInterface->isValid()) {
        m_lastError = "Failed to connect to PolicyKit service";
        qWarning() << "Polkit DBus interface invalid:" << m_polkitInterface->lastError();
    }

    if (!QDBusConnection::systemBus().isConnected()) {
        m_lastError = "Failed to connect to system DBus";
        qCritical() << m_lastError;
        return;
    }

    // Check if PolicyKit service is available
    QDBusConnectionInterface* busInterface = QDBusConnection::systemBus().interface();
    if (!busInterface || !busInterface->isServiceRegistered("org.freedesktop.PolicyKit1")) {
        m_lastError = "PolicyKit service not available";
        qCritical() << m_lastError;
        return;
    }

    // Check initial authorization
    checkAuthorization();
}

PolkitManager::~PolkitManager() = default;

bool
PolkitManager::isAuthorized() const
{
    return m_authorized;
}

bool
PolkitManager::isSessionEnabled() const
{
    return m_sessionEnabled;
}

void
PolkitManager::setSessionEnabled(bool enabled)
{
    if (m_sessionEnabled != enabled) {
        DEBUG_LOG("Session management changed from ", m_sessionEnabled, " to ", enabled);
        m_sessionEnabled = enabled;
        
        // Save to settings
        QSettings settings;
        settings.setValue("polkit/session_enabled", enabled);
        
        if (!enabled) {
            // Clear current session
            clearSession();
        }
        
        emit sessionEnabledChanged(enabled);
    }
}

void
PolkitManager::clearSession()
{
    DEBUG_LOG("Clearing authentication session");
    m_authenticationCookie.clear();
    
    // Force re-check authorization
    bool wasAuthorized = m_authorized;
    m_authorized = false;
    
    if (wasAuthorized) {
        emit authorizationChanged(false);
        emit sessionCleared();
    }
}

bool
PolkitManager::requestInitialAuthentication()
{
    if (isAuthorized()) {
        return true;
    }

    // Show progress during authentication
    QWidget *parentWidget = qobject_cast<QWidget*>(parent());
    QProgressDialog *progress = nullptr;
    
    if (parentWidget) {
        QString message = m_sessionEnabled ? 
            "Requesting authentication session..." : 
            "Requesting authentication...";
        progress = new QProgressDialog(message, QString(), 0, 0, parentWidget);
        progress->setModal(true);
        progress->setCancelButton(nullptr);
        progress->setMinimumDuration(0);
        progress->show();
        QApplication::processEvents();
    }

    bool success = requestAuthorization("Authentication required to access btrfs filesystem information");
    DEBUG_LOG("Initial auth result: ",
          success ? "authorized" : "not authorized",
          m_lastError.isEmpty() ? " (no error)" : QString(" Error: %1").arg(m_lastError));
    
    if (progress) {
        progress->close();
        progress->deleteLater();
    }

    return success;
}

bool
PolkitManager::checkAuthorization()
{
    if (!m_polkitInterface || !m_polkitInterface->isValid()) {
        m_lastError = "Polkit interface not available";
        DEBUG_LOG("Polkit interface not valid");
        return false;
    }

    // If we have a session cookie and session is enabled, we might still be authorized
    if (m_sessionEnabled && !m_authenticationCookie.isEmpty()) {
        DEBUG_LOG("Using cached session authorization");
        return m_authorized;
    }

    // Prepare subject with CORRECT type and start time
    PolkitSubject subject;
    subject.kind = "unix-process";

    // Use explicit type conversions
    quint32 pid = static_cast<quint32>(getpid());
    quint64 startTime = static_cast<quint64>(getProcessStartTime());

    subject.details["pid"] = pid;
    subject.details["start-time"] = startTime;
    
    // Create empty details map
    StringMap emptyDetails;
    
    // Prepare D-Bus message
    QDBusMessage dbusMessage = QDBusMessage::createMethodCall(
        "org.freedesktop.PolicyKit1",
        "/org/freedesktop/PolicyKit1/Authority",
        "org.freedesktop.PolicyKit1.Authority",
        "CheckAuthorization"
    );
    
    // Build arguments with correct types
    dbusMessage << QVariant::fromValue(subject);  // Subject struct (sa{sv})
    dbusMessage << m_actionId;                    // Action ID (s)
    dbusMessage << QVariant::fromValue(emptyDetails);  // Details (a{ss})
    dbusMessage << static_cast<quint32>(0);       // Flags (u)
    dbusMessage << QString();                     // Cancellation ID (s)

    DEBUG_LOG("Sending CheckAuthorization request:");
    DEBUG_LOG("  Subject: ", subject.kind);
    DEBUG_LOG("  PID: ", pid);
    DEBUG_LOG("  Start Time: ", startTime);
    DEBUG_LOG("  Action ID: ", m_actionId);

    // Send message using QDBusInterface for better type handling
    // Use the correct signature with three separate arguments: bba{ss}
    QDBusPendingReply<CheckAuthorizationResult> reply = 
        m_polkitInterface->asyncCallWithArgumentList("CheckAuthorization", dbusMessage.arguments());
    
    // Wait for the reply with timeout
    reply.waitForFinished();
    
    if (reply.isError()) {
        QDBusError error = reply.error();
        m_lastError = QString("DBus error: %1 - %2").arg(error.name(), error.message());
        DEBUG_LOG("DBus Error Details:");
        DEBUG_LOG("  Error name: ", error.name());
        DEBUG_LOG("  Error message: ", error.message());
        return false;
    }

    // Extract the values directly
    CheckAuthorizationResult result = reply.value();
    bool authorized = result.authorized;
    bool challenge = result.challenge;
    StringMap replyDetails = result.details;

    bool wasAuthorized = m_authorized;
    m_authorized = authorized;
    
    DEBUG_LOG("Authorization result:");
    DEBUG_LOG("  Authorized: ", authorized ? "authorized" : "not authorized");
    DEBUG_LOG("  Challenge: ", challenge ? "user interaction required" : "no further challenge");

    // For session mode, we maintain our own session
    if (authorized && m_sessionEnabled) {
        m_authenticationCookie = QString("session_%1_%2")
            .arg(QDateTime::currentSecsSinceEpoch())
            .arg(getpid());
        DEBUG_LOG("Starting application session with cookie: ", m_authenticationCookie);
        emit sessionStarted();
    }

    if (wasAuthorized != m_authorized) {
        DEBUG_LOG("Authorization changed to: ", m_authorized);
        emit authorizationChanged(m_authorized);
    }
    
    return m_authorized;
}

bool
PolkitManager::requestAuthorization(const QString &message)
{
    // We can still emit the message for UI purposes, even though we can't pass it to Polkit
    QString authMessage = message.isEmpty() ? 
        "Authentication required for beekeeper filesystem operations" : message;
    
    emit authenticationRequired(authMessage);
    
    // Perform authentication without custom message
    return performAuthentication(authMessage);
}

bool
PolkitManager::performAuthentication(const QString &message)
{
    Q_UNUSED(message);
    
    if (!m_polkitInterface || !m_polkitInterface->isValid()) {
        m_lastError = "Polkit interface not available";
        DEBUG_LOG("Polkit interface not valid in performAuthentication");
        return false;
    }

    // Prepare subject with CORRECT type and start time
    PolkitSubject subject;
    subject.kind = "unix-process";
    subject.details["pid"] = static_cast<quint32>(getpid());
    subject.details["start-time"] = static_cast<quint64>(getProcessStartTime());
    
    DEBUG_LOG("Authentication request details:");
    DEBUG_LOG("  PID: ", subject.details["pid"]);
    DEBUG_LOG("  Start Time: ", subject.details["start-time"]);
    DEBUG_LOG("  Action ID: ", m_actionId);
    DEBUG_LOG("  Session enabled: ", m_sessionEnabled);
    
    // Create empty details map for the call
    StringMap emptyDetails;
    
    // Prepare D-Bus message
    QDBusMessage dbusMessage = QDBusMessage::createMethodCall(
        "org.freedesktop.PolicyKit1",
        "/org/freedesktop/PolicyKit1/Authority",
        "org.freedesktop.PolicyKit1.Authority",
        "CheckAuthorization"
    );
    
    // Build arguments with correct types
    dbusMessage << QVariant::fromValue(subject);  // Subject struct (sa{sv})
    dbusMessage << m_actionId;                    // Action ID (s)
    dbusMessage << QVariant::fromValue(emptyDetails);  // Details (a{ss})
    dbusMessage << static_cast<quint32>(1);       // Flags: ALLOW_USER_INTERACTION (u)
    dbusMessage << QString();                     // Cancellation ID (s)

    // Use the correct signature with three separate arguments
    QDBusPendingReply<CheckAuthorizationResult> reply = 
        m_polkitInterface->asyncCallWithArgumentList("CheckAuthorization", dbusMessage.arguments());
    
    // Wait for the reply with timeout
    reply.waitForFinished();
    
    if (reply.isError()) {
        QDBusError error = reply.error();
        m_lastError = QString("DBus error: %1 - %2").arg(error.name(), error.message());
        DEBUG_LOG("DBus Error in performAuthentication:");
        DEBUG_LOG("  Error name: ", error.name());
        DEBUG_LOG("  Error message: ", error.message());
        return false;
    }

    // Extract the values directly
    CheckAuthorizationResult result = reply.value();
    bool authorized = result.authorized;
    bool challenge = result.challenge;
    StringMap replyDetails = result.details;

    bool wasAuthorized = m_authorized;
    m_authorized = authorized;
    
    DEBUG_LOG("Authorization result:");
    DEBUG_LOG("  Authorized: ", authorized ? "authorized" : "not authorized");
    DEBUG_LOG("  Challenge: ", challenge ? "user interaction required" : "no further challenge");

    // For session mode, we maintain our own session
    if (authorized && m_sessionEnabled) {
        m_authenticationCookie = QString("session_%1_%2")
            .arg(QDateTime::currentSecsSinceEpoch())
            .arg(getpid());
        DEBUG_LOG("Starting application session with cookie: ", m_authenticationCookie);
        emit sessionStarted();
    }

    if (wasAuthorized != m_authorized) {
        DEBUG_LOG("Authorization changed to: ", m_authorized);
        emit authorizationChanged(m_authorized);
    }
    
    return m_authorized;
}

pid_t
PolkitManager::runPrivilegedHelper(const QString &helperPath, const QStringList &args) {
    DEBUG_LOG("Executing helper with arguments:", args.join(" ").toStdString());
    pid_t childPid = fork();
    if (childPid == 0) {
        // Child process: exec pkexec with helper + args
        QByteArray helperUtf8 = helperPath.toUtf8();

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>("pkexec"));
        argv.push_back(const_cast<char*>(helperUtf8.constData()));

        std::vector<QByteArray> utf8Args;
        utf8Args.reserve(args.size());
        for (const auto &arg : args) {
            utf8Args.push_back(arg.toUtf8());
            argv.push_back(const_cast<char*>(utf8Args.back().constData()));
        }

        argv.push_back(nullptr);

        execvp("pkexec", argv.data());
        perror("execvp(pkexec, helper) failed");
        _exit(127);
    }

    if (childPid < 0) {
        qWarning() << "fork() failed:" << strerror(errno);
        return -1;
    }

    return childPid;
}


} // namespace auth
} // namespace beekeeper