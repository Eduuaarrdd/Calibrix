#ifndef PLAN_GENERATOR_H
#define PLAN_GENERATOR_H

#include <QVector>
#include <QtGlobal>
#include <QtCore/QByteArray>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <limits>
#include <cmath>
#include <QSharedPointer>

#include "settingsmanager.h"
#include "typemeasurement.h"

// ─────────────────────────────────────────────────────
// Общие константы
// ─────────────────────────────────────────────────────
namespace PlanGenConst {
constexpr int    kStepNone = -1;        // sentinel для режима NONE
constexpr double kEps      = 1e-9;      // эпсилон для направления
}

// ─────────────────────────────────────────────────────
// Действия шага в плане сохранений
// ─────────────────────────────────────────────────────
enum class SaveAction {
    Measurement = 0,  // обычное измерение
    NewStep     = 1,  // первый элемент нового номера шага
    NewGroup    = 2   // старт новой группы
};

// ─────────────────────────────────────────────────────
// Элемент базового плана
// ─────────────────────────────────────────────────────
struct PlanBaseItem {
    int                 stepNumber = PlanGenConst::kStepNone;              // номер шага или kStepNone
    double              expected   = std::numeric_limits<double>::quiet_NaN(); // ожидаемое
    int                 repeats    = 0;                                    // количество измерений на шаг
    ApproachDirection   direction  = ApproachDirection::Unknown;           // направление шага
};

// ─────────────────────────────────────────────────────
// Базовый план
// ─────────────────────────────────────────────────────
struct PlanBase {
    QVector<PlanBaseItem> items;   // уникальные шаги 1..N или один NONE
    bool                  bidirectional = false; // флаг bidi из настроек

    quint64 baseStructureHash = 0; // хэш структуры
    double base = 0.0; // базовая точка
};

// ─────────────────────────────────────────────────────
// Элемент плана сохранений
// ─────────────────────────────────────────────────────
struct PlanSaveItem {
    int               stepNumber = PlanGenConst::kStepNone;                  // номер шага
    double            expected   = std::numeric_limits<double>::quiet_NaN(); // ожидаемое
    int               address    = 0;                                        // адрес уникального шага
    SaveAction        actionFirst = SaveAction::Measurement;                 // действие на ПЕРВОМ проходе
    SaveAction        action     = SaveAction::Measurement;                  // действие
    ApproachDirection direction  = ApproachDirection::Unknown;               // направление шага
};

// ─────────────────────────────────────────────────────
// План сохранений
// ─────────────────────────────────────────────────────
struct PlanSave {
    QVector<PlanSaveItem> items;
    bool bidirectional = false;
    quint64 baseStructureHash = 0; // скопировано из Base
     double base = 0.0; // базовая точка
};

// ─────────────────────────────────────────────────────
// Генератор планов
// ─────────────────────────────────────────────────────
class PlanGenerator
{
public:
    PlanGenerator() = default;
    // ─────────────────────────────────────────────────────
    // Создание базового плана
    // ─────────────────────────────────────────────────────
    void makeBase(const StepSettings& step);

    // ─────────────────────────────────────────────────────
    // Создание плана сохранений на основе Base
    // Возвращает лёгкую ручку на const-план (без копий данных)
    // ─────────────────────────────────────────────────────
    QSharedPointer<const PlanSave> makeSave();


    // Доступ только для чтения к текущему Base (если понадобится)
    const PlanBase& currentBase() const { return base_; }

private:
    // ─────────────────────────────────────────────────────
    // Хелперы: хэши
    // ─────────────────────────────────────────────────────
    static quint64 hash64(const QByteArray& bytes);               // первые 8 байт SHA-256
    static quint64 hashBaseStructure(const PlanBase& base);       // структура: N, repeats, bidi, stepNumber
    static quint64 hashBaseValues(const PlanBase& base);          // структура + expected

    // ─────────────────────────────────────────────────────
    // Хелперы: развёртки и модификаторы
    // ─────────────────────────────────────────────────────
    PlanSave schemeGenA() const;             // 1 1 1 2 2 2 ...
    static void     forwardGen(PlanSave&);                   // первая ячейка -> NewGroup
    static void     bidiGen(PlanSave&);                      // зеркалирование stepNumber+expected

    // ─────────────────────────────────────────────────────
    // Хелперы: утилиты
    // ─────────────────────────────────────────────────────
    static ApproachDirection directionByDiff(double prev, double curr); // знак разности
    static int               maxAddress(const QVector<PlanSaveItem>& v);// максимум адреса
    static ApproachDirection invertDirection(ApproachDirection d);//зеркаклим
    // ─────────────────────────────────────────────────────
    // Приватный текущий Base (единственный источник правды)
    // ─────────────────────────────────────────────────────
    PlanBase base_{};

    QSharedPointer<const PlanSave> cachedSave_;
};

#endif // PLAN_GENERATOR_H
