#ifndef PYPROC_H
#define PYPROC_H

#include <QObject>
#include <QProcess>
#include <QString>

class PyProc : public QObject
{
    Q_OBJECT
public:
    explicit PyProc(QObject *parent = nullptr);

    void start(const QString& scriptPath);
    void stop();
    bool isRunning() const;

signals:
    void started();
    void stopped();
    void error(const QString& error);
    void distance(double value);

private slots:
    void onReadyReadStdOut();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess* m_proc;
};

#endif // PYPROC_H

