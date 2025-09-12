#include "plangenerator.h"
#include <algorithm>
#include <QBuffer>

#include "calculatemesurement.h"

// ─────────────────────────────────────────────────────
// Хелпер: 8 байт SHA-256
// ─────────────────────────────────────────────────────
quint64 PlanGenerator::hash64(const QByteArray& bytes)
{
    const QByteArray h = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
    quint64 out = 0;
    for (int i = 0; i < 8 && i < h.size(); ++i) out = (out << 8) | static_cast<unsigned char>(h.at(i));
    return out;
}

// ─────────────────────────────────────────────────────
// Хэши базового плана: структура
// ─────────────────────────────────────────────────────
quint64 PlanGenerator::hashBaseStructure(const PlanBase& base)
{
    QByteArray buf;
    QDataStream ds(&buf, QIODevice::WriteOnly);
    ds.setVersion(QDataStream::Qt_6_0);

    ds << static_cast<qint8>(base.bidirectional ? 1 : 0);
    ds << static_cast<quint32>(base.items.size());

    for (const auto& it : base.items) {
        ds << static_cast<qint32>(it.stepNumber);
        ds << static_cast<qint32>(it.repeats);
    }
    return hash64(buf);
}

// ─────────────────────────────────────────────────────
// Хэши базового плана: структура + expected
// ─────────────────────────────────────────────────────
quint64 PlanGenerator::hashBaseValues(const PlanBase& base)
{
    QByteArray buf;
    QDataStream ds(&buf, QIODevice::WriteOnly);
    ds.setVersion(QDataStream::Qt_6_0);

    ds << static_cast<qint8>(base.bidirectional ? 1 : 0);
    ds << static_cast<quint32>(base.items.size());

    for (const auto& it : base.items) {
        ds << static_cast<qint32>(it.stepNumber);
        ds << static_cast<qint32>(it.repeats);
        ds << static_cast<double>(it.expected);
    }
    return hash64(buf);
}

// ─────────────────────────────────────────────────────
// Утилита: направление по разности
// ─────────────────────────────────────────────────────
ApproachDirection PlanGenerator::directionByDiff(double prev, double curr)
{
    const double d = curr - prev;
    if (d > PlanGenConst::kEps)  return ApproachDirection::Forward;
    if (d < -PlanGenConst::kEps) return ApproachDirection::Backward;
    return ApproachDirection::Unknown;
}

// ─────────────────────────────────────────────────────
// Утилита: максимум адреса
// ─────────────────────────────────────────────────────
int PlanGenerator::maxAddress(const QVector<PlanSaveItem>& v)
{
    int m = 0;
    for (const auto& x : v) m = std::max(m, x.address);
    return m;
}

// ─────────────────────────────────────────────────────
// Создание базового плана
// ─────────────────────────────────────────────────────
void PlanGenerator::makeBase(const StepSettings& step)
{
    PlanBase out;
    out.bidirectional = step.bidirectional;

    const bool badCounts = (step.count <= 0) || (step.repeatCount <= 0);

    if (step.mode == StepMode::None || badCounts) {
        PlanBaseItem item;
        item.stepNumber = PlanGenConst::kStepNone;
        item.expected   = std::numeric_limits<double>::quiet_NaN();
        item.repeats    = std::max(0, step.repeatCount);
        item.direction  = ApproachDirection::Unknown;
        out.items.push_back(item);

        out.baseStructureHash = hashBaseStructure(out);
        out.base = step.base;

        base_ = std::move(out);
        cachedSave_.reset();
        return;
    }

    out.items.reserve(step.count);

    double prevExpected = step.base;

    QVector<double> manualList;
    if (step.mode == StepMode::Manual) {
        manualList = CalculateMesurement::parseManual(step.manualText);
    }

    for (int i = 1; i <= step.count; ++i) {
        PlanBaseItem it;
        it.stepNumber = i;

        double exp = std::numeric_limits<double>::quiet_NaN();

        switch (step.mode) {
        case StepMode::Uniform:
            exp = CalculateMesurement::expectedUniform(i, step.step, step.base);
            break;
        case StepMode::Manual:
            // берём из заранее распарсенного списка
            exp = CalculateMesurement::expectedManualFromList(i, manualList, step.base);
            break;
        case StepMode::Formula:
            exp = CalculateMesurement::expectedFormula(i, step.formula, step.formulaCount, step.base); // пока 0.0
            break;
        case StepMode::None:
        default:
            exp = std::numeric_limits<double>::quiet_NaN();
            break;
        }

        it.expected = exp;
        it.repeats    = step.repeatCount;
        it.direction  = directionByDiff(prevExpected, it.expected);
        prevExpected  = it.expected;
        out.items.push_back(it);
    }

    out.baseStructureHash = hashBaseStructure(out);
    out.base = step.base;

    // Публикуем новый Base
    base_ = std::move(out);

    // инвалидируем кэш производных
    cachedSave_.reset();
    return;
}

// ─────────────────────────────────────────────────────
// schemeGenA: 1 1 1 2 2 2 ... с адресами и действиями
// ─────────────────────────────────────────────────────
PlanSave PlanGenerator::schemeGenA() const
{
    PlanSave save;

    if (base_.items.isEmpty()) return save;

    int currentAddress = 0;
    bool firstItem = true;

    for (int idx = 0; idx < base_.items.size(); ++idx) {
        const auto& b = base_.items[idx];

        const int repeats = std::max(0, b.repeats);
        for (int r = 0; r < repeats; ++r) {
            PlanSaveItem s;
            s.stepNumber = b.stepNumber;
            s.expected   = b.expected;
            s.address    = currentAddress;
            s.direction  = b.direction;

            // действие на первом проходе
            if (firstItem) {
                s.actionFirst = SaveAction::NewGroup;   // самый первый элемент открывает группу
                firstItem = false;
            } else if (r == 0) {
                s.actionFirst = SaveAction::NewStep;    // первый повтор шага — старт шага
            } else {
                s.actionFirst = SaveAction::Measurement;// остальные — просто измерение
            }

            // действие на последующих проходах — ВСЕГДА измерение

            if (b.stepNumber == PlanGenConst::kStepNone) {
                s.action = SaveAction::NewStep;
            } else {
                s.action = SaveAction::Measurement;
            }

            save.items.push_back(s);
        }

        if (idx + 1 < base_.items.size()) currentAddress += 1;
    }

    return save;
}

// ─────────────────────────────────────────────────────
// forwardGen: первая ячейка -> NewGroup
// ─────────────────────────────────────────────────────
void PlanGenerator::forwardGen(PlanSave& save)
{
    if (!save.items.isEmpty())
        save.items[0].actionFirst = SaveAction::NewGroup;
}


// ─────────────────────────────────────────────────────
// bidiGen: зеркалим stepNumber+expected, action/address по местам
// ─────────────────────────────────────────────────────
void PlanGenerator::bidiGen(PlanSave& save)
{
    if (save.items.isEmpty()) return;


    QVector<PlanSaveItem> mirror;
    mirror.reserve(save.items.size());

    const int offset = maxAddress(save.items) + 1;

    for (int i = 0; i < save.items.size(); ++i) {
        const auto& srcSameIndex = save.items[i];
        const auto& srcRevIndex  = save.items[save.items.size() - 1 - i];

        PlanSaveItem m;

        m.stepNumber = srcRevIndex.stepNumber;
        m.expected   = srcRevIndex.expected;
        m.direction  = invertDirection(srcRevIndex.direction);

        m.actionFirst = srcSameIndex.actionFirst;
        m.action      = srcSameIndex.action;
        m.address     = srcSameIndex.address + offset;

        mirror.push_back(m);
    }

    save.items += mirror;
}

// ─────────────────────────────────────────────────────
// Создание плана сохранений на основе Base
// ─────────────────────────────────────────────────────

QSharedPointer<const PlanSave> PlanGenerator::makeSave()
{
    // Если Base ещё не инициализирован — вернём пустую ручку
    if (base_.items.isEmpty() && base_.baseStructureHash == 0) {
        return {};
    }

    // Если кэш валиден — отдать его
    if (cachedSave_
        && cachedSave_->baseStructureHash == base_.baseStructureHash
        && cachedSave_->base == base_.base) {
        return cachedSave_;
    }
    enum class SaveCase { Uni = 0, Bidi = 1 };

    const SaveCase sc = base_.bidirectional ? SaveCase::Bidi : SaveCase::Uni;

    PlanSave out;

    switch (sc) {
    case SaveCase::Uni:
        out = schemeGenA();
        forwardGen(out);
        out.bidirectional = false;
        break;
    case SaveCase::Bidi:
        out = schemeGenA();
        bidiGen(out);
        out.bidirectional = true;
        break;
    }

    out.bidirectional     = base_.bidirectional;
    out.baseStructureHash = base_.baseStructureHash;
    out.base              = base_.base;

    // Кладём в кэш и возвращаем лёгкую ручку
    cachedSave_ = QSharedPointer<const PlanSave>::create(std::move(out));
    return cachedSave_;
}

// ─────────────────────────────────────────────────────
    // invertDirection: Forward <-> Backward (Unknown — без изменений)
    // ─────────────────────────────────────────────────────
    ApproachDirection PlanGenerator::invertDirection(ApproachDirection d)
    {
        switch (d) {
        case ApproachDirection::Forward:  return ApproachDirection::Backward;
        case ApproachDirection::Backward: return ApproachDirection::Forward;
        case ApproachDirection::Unknown:
            default: return ApproachDirection::Unknown;
            }
    }
