#include "Logger.hpp"
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <iostream>

// ── Singleton ─────────────────────────────────────────────────────────────────

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() {
    // Build log directory: ~/.config/cimg-viewer/logs/
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!base.endsWith("cimg-viewer")) base += "/cimg-viewer";
    m_logDir = base + "/logs";
    QDir().mkpath(m_logDir);

    rotateLogs();
    openLogFile();

    log(LogLevel::Info, "Logger",
        QString("Session started — log: %1").arg(m_file.fileName()));
}

Logger::~Logger() {
    if (m_file.isOpen()) {
        log(LogLevel::Info, "Logger", "Session ended");
        m_file.close();
    }
}

// ── File management ───────────────────────────────────────────────────────────

void Logger::rotateLogs() {
    // Keep at most 7 log files (sorted by name = by date, oldest first)
    QDir dir(m_logDir);
    QStringList files = dir.entryList({"cimg-viewer-*.log"}, QDir::Files, QDir::Name);
    while (files.size() >= 7) {
        dir.remove(files.takeFirst());
    }
}

void Logger::openLogFile() {
    QString date = QDate::currentDate().toString("yyyy-MM-dd");
    m_currentDate = date;
    QString path  = m_logDir + "/cimg-viewer-" + date + ".log";
    m_file.setFileName(path);
    m_file.open(QIODevice::Append | QIODevice::Text);
}

QString Logger::currentLogPath() const {
    QMutexLocker lk(&m_mutex);
    return m_file.fileName();
}

// ── Core log method ───────────────────────────────────────────────────────────

void Logger::log(LogLevel level, const QString& module, const QString& msg) {
    static const char* levelNames[] = {
        "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
    };
    int idx = std::min((int)level, 4);

    QMutexLocker lk(&m_mutex);

    // Date-rollover: if the calendar date has changed, open a new log file
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    if (today != m_currentDate) {
        m_file.close();
        rotateLogs();
        openLogFile();
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString line = QString("[%1] [%2] [%3] %4\n")
                       .arg(timestamp)
                       .arg(levelNames[idx])
                       .arg(module)
                       .arg(msg);

    if (m_file.isOpen()) {
        m_file.write(line.toUtf8());
        m_file.flush();
    }

    // Mirror WARNING and above to stderr
    if (level >= LogLevel::Warning)
        std::cerr << line.toStdString();
}
