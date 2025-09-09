#include "mainwindow.hpp"
#include <QVBoxLayout>

// Show debug logs (only enabled with CMake flag)
#ifdef BEEKEEPER_DEBUG_LOGGING
#include <QFontDatabase>
#include <QScrollBar>
#include <QTextEdit>

static
QString formatLogLine(const QString &line)
{
    static QRegularExpression re(R"(^(\[[^\]]+\])\s*([^:]+:)(.*)$)");
    auto m = re.match(line);

    if (m.hasMatch()) {
        return QString("<span style='font-weight:bold; color:blue;'>%1</span> "
                       "<span style='font-weight:bold;'>%2</span>%3")
                   .arg(m.captured(1).toHtmlEscaped(),
                        m.captured(2).toHtmlEscaped(),
                        m.captured(3).toHtmlEscaped());
    }
    return line.toHtmlEscaped();
}

void
MainWindow::showDebugLog()
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle("Beekeeper Debug Log");
    dialog->resize(900, 600);

    auto *layout = new QVBoxLayout(dialog);
    auto *logView = new QTextEdit(dialog);
    logView->setReadOnly(true);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    logView->setFont(monoFont);
    layout->addWidget(logView);

    // Auto-scroll control
    auto *autoScrollButton = new QPushButton("Auto scrolling");
    autoScrollButton->setCheckable(true);
    autoScrollButton->setChecked(true); // enabled by default
    layout->addWidget(autoScrollButton);

    QString logPath = "/tmp/beekeeper-debug.log";
    auto *file = new QFile(logPath, dialog);
    if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        logView->setPlainText("Failed to open debug log at " + logPath);
    } else {
        QTextStream in(file);
        while (!in.atEnd()) {
            logView->append(formatLogLine(in.readLine()));
        }
    }

    // Track whether user scrolled up
    QObject::connect(logView->verticalScrollBar(), &QScrollBar::valueChanged,
                     dialog, [logView, autoScrollButton](int) {
        QScrollBar *sb = logView->verticalScrollBar();
        bool atBottom = (sb->value() == sb->maximum());
        if (!atBottom) {
            autoScrollButton->setChecked(false); // disable auto-scroll
        }
    });

    // Periodically append new log lines
    auto *timer = new QTimer(dialog);
    QObject::connect(timer, &QTimer::timeout, dialog, [file, logView, autoScrollButton]() {
        if (!file->isOpen()) return;
        while (!file->atEnd()) {
            QString line = QString::fromUtf8(file->readLine()).trimmed();
            if (!line.isEmpty()) {
                logView->append(formatLogLine(line));
            }
        }
        // Scroll only if auto-scroll enabled
        if (autoScrollButton->isChecked()) {
            logView->moveCursor(QTextCursor::End);
        }
    });
    timer->start(1000);

    dialog->show();
}
#endif
