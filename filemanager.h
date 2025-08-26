#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QString>

class QWidget;
class QAbstractItemModel;

class FileManager : public QObject {
    Q_OBJECT
public:
    explicit FileManager(QObject* parent = nullptr);

    // Экспорт модели в CSV
    void exportToCsv(QWidget* parent, const QAbstractItemModel* model);

signals:
    void exportSuccess();
    void exportFailure(const QString& error);
};

#endif // FILEMANAGER_H

