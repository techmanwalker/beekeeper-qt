// include/beekeeper/qt-debug.hpp
#pragma once
#ifdef BEEKEEPER_DEBUG_LOGGING

#include "debug.hpp"
#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QList>
#include <QDebug>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QVariantMap>

// Overloads for Qt types
inline void debug_print(std::ostream& os, const QString& value) {
    os << value.toStdString();
}

inline void debug_print(std::ostream& os, const QByteArray& value) {
    os << value.constData();
}

inline void debug_print(std::ostream& os, const QVariant& value) {
    os << value.toString().toStdString();
}

inline void debug_print(std::ostream& os, bool value) {
    os << (value ? "true" : "false");
}

inline void debug_print(std::ostream& os, const QDBusMessage& value) {
    os << "QDBusMessage(type=" << value.type() 
       << ", service=" << value.service().toStdString()
       << ", path=" << value.path().toStdString()
       << ", interface=" << value.interface().toStdString()
       << ", member=" << value.member().toStdString() << ")";
}

inline void debug_print(std::ostream& os, const QDBusArgument& arg) {
    os << "QDBusArgument(type=" << arg.currentType() << ")";
}

inline void debug_print(std::ostream& os, const QVariantMap& map) {
    os << "QVariantMap{";
    bool first = true;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (!first) os << ", ";
        os << it.key().toStdString() << ": ";
        debug_print(os, it.value());
        first = false;
    }
    os << "}";
}

inline void debug_print(std::ostream& os, const QMap<QString, QString>& map) {
    os << "StringMap{";
    bool first = true;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (!first) os << ", ";
        os << it.key().toStdString() << ": " << it.value().toStdString();
        first = false;
    }
    os << "}";
}

template<typename T>
inline void debug_print(std::ostream& os, const QList<T>& list) {
    os << "QList[";
    for (int i = 0; i < list.size(); ++i) {
        if (i > 0) os << ", ";
        debug_print(os, list[i]);
    }
    os << "]";
}

#endif // BEEKEEPER_DEBUG_LOGGING