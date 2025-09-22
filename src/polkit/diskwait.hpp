#pragma once

#include <QThread>
#include <QString>

class diskwait : public QThread
{
    Q_OBJECT

public:
    explicit diskwait(QObject *parent = nullptr);
    ~diskwait() override;

protected:
    void run() override;
};
