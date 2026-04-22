#pragma once
#include <QString>
#include <QMutex>
#include <QFile>

enum class LogLevel { Debug = 0, Info, Warning, Error, Critical };

class Logger {
public:
    static Logger& instance();

    void log(LogLevel level, const QString& module, const QString& msg);

    QString currentLogPath() const;
    QString logDir() const { return m_logDir; }

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void openLogFile();
    void rotateLogs();   // keep at most 7 log files

    mutable QMutex m_mutex;
    QFile   m_file;
    QString m_logDir;
    QString m_currentDate;
};

// ── Convenience macros ────────────────────────────────────────────────────────
#define LOG_DEBUG(mod, msg)    Logger::instance().log(LogLevel::Debug,    (mod), (msg))
#define LOG_INFO(mod, msg)     Logger::instance().log(LogLevel::Info,     (mod), (msg))
#define LOG_WARNING(mod, msg)  Logger::instance().log(LogLevel::Warning,  (mod), (msg))
#define LOG_ERROR(mod, msg)    Logger::instance().log(LogLevel::Error,    (mod), (msg))
#define LOG_CRITICAL(mod, msg) Logger::instance().log(LogLevel::Critical, (mod), (msg))
