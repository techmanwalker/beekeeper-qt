#pragma once
#include <QObject>
#include <QString>

// Functions that return text for the help dialogs
class helptexts : public QObject {
    Q_OBJECT
public:
    QString keyboardnav ();
    QString what_is_beekeeper_qt();
};
