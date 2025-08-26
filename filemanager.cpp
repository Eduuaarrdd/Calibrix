#include "filemanager.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QAbstractItemModel>

FileManager::FileManager(QObject* parent)
    : QObject(parent) {}

void FileManager::exportToCsv(QWidget* parent, const QAbstractItemModel* model)
{
    if (!model) {
        emit exportFailure("Пустая модель.");
        QMessageBox::warning(parent, "Ошибка", "Пустая модель.");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(parent, "Сохранить как CSV", "", "CSV файлы (*.csv)");
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit exportFailure("Не удалось открыть файл для записи.");
        QMessageBox::warning(parent, "Ошибка", "Не удалось открыть файл для записи.");
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8); // Qt 6
    out << QChar(0xFEFF);

    // Заголовок
    out << "№;Значение\n";

    for (int row = 0; row < model->rowCount(); ++row) {
        QString value = model->data(model->index(row, 0)).toString();
        out << (row + 1) << ";" << value << "\n";
    }

    file.close();
    emit exportSuccess();
    QMessageBox::information(parent, "Готово", "Файл успешно экспортирован.");
}
