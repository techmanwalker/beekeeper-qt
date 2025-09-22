#pragma once

#include <QObject>
#include <QDBusContext>
#include <QVariantMap>
#include <QStringList>

class masterservice : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.beekeeper.Helper")
public:
    explicit masterservice(QObject *parent = nullptr);
    ~masterservice();

public slots:
    QVariantMap ExecuteCommand(const QString &verb,
                               const QVariantMap &options,
                               const QStringList &subjects);

private:
    // Helpers: conversion utilities used by ExecuteCommand
    static std::map<std::string, std::string> convert_options(const QVariantMap &options);
    static std::vector<std::string> convert_subjects(const QStringList &subjects);
};
