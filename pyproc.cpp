#include "pyproc.h"
#include <QJsonDocument>
#include <QJsonObject>

PyProc::PyProc(QObject *parent)
    : QObject(parent), m_proc(nullptr)
{}

void PyProc::start(const QString& scriptPath)
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        emit error("Python process already running");
        return;
    }
    m_proc = new QProcess(this);

    connect(m_proc, &QProcess::readyReadStandardOutput, this, &PyProc::onReadyReadStdOut);
    connect(m_proc, &QProcess::errorOccurred, this, &PyProc::onProcessError);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PyProc::onProcessFinished);

    // Очищаем переменные среды для избежания конфликтов с Python
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("PYTHONHOME");
    env.remove("PYTHONPATH");
    m_proc->setProcessEnvironment(env);

    QString pythonExe = "python"; // Или укажи полный путь до python.exe, если нужно
    m_proc->start(pythonExe, QStringList() << scriptPath);

    if (!m_proc->waitForStarted(1000)) {
        emit error("Failed to start python process");
        m_proc->deleteLater();
        m_proc = nullptr;
        return;
    }
    emit started();
}

void PyProc::stop()
{
    if (m_proc) {
        disconnect(m_proc, nullptr, this, nullptr);
        m_proc->kill();
        m_proc->waitForFinished(1000);
        m_proc->deleteLater();
        m_proc = nullptr;
        emit stopped();
    }
}

bool PyProc::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void PyProc::onReadyReadStdOut()
{
    if (!m_proc) return;
    while (m_proc->canReadLine()) {
        QByteArray line = m_proc->readLine().trimmed();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;
        double val = doc.object().value("distance").toDouble();
        emit distance(val);
    }
}

void PyProc::onProcessError(QProcess::ProcessError error)
{
    QString msg;
    switch (error) {
    case QProcess::FailedToStart: msg = "Python process failed to start"; break;
    case QProcess::Crashed: msg = "Python process crashed"; break;
    case QProcess::Timedout: msg = "Python process timed out"; break;
    case QProcess::WriteError: msg = "Python process write error"; break;
    case QProcess::ReadError: msg = "Python process read error"; break;
    default: msg = "Unknown error in Python process";
    }
    emit this->error(msg);
}

void PyProc::onProcessFinished(int, QProcess::ExitStatus)
{
    emit stopped();
    if (m_proc) {
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

