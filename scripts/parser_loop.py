#!/usr/bin/env python3
import json
import random
import time

# ------------------ НАСТРОЙКИ (мм и Гц) ------------------

start_mm = 0            # начало диапазона (мм)
end_mm = 100            # конец диапазона (мм)
step_mm = 20            # шаг между точками (мм)
feed_mm_min = 200       # подача (мм/мин)
wait_at_point_s = 10     # время стоянки на точке (сек)
wait_back_s = 2         # время стоянки после отката (сек)
hz = 30                 # частота обновления (Гц)
measurements_per_step = 5 # кол-во подходов на каждую точку

# шум (мм)
noise_sigma_mm = 0.002
noise_low_mm   = -0.002
noise_high_mm  =  0.002

# погрешность при остановке (мм)
error_sigma_mm = 0.05
error_low_mm   = -0.05
error_high_mm  =  0.05

# ------------------ КОНВЕРТАЦИЯ В МЕТРЫ ------------------

start_m = start_mm / 1000.0
end_m   = end_mm   / 1000.0
step_m  = step_mm  / 1000.0
feed_m_per_sec = (feed_mm_min / 1000.0) / 60.0

noise_sigma_m = noise_sigma_mm / 1000.0
noise_low_m   = noise_low_mm   / 1000.0
noise_high_m  = noise_high_mm  / 1000.0

error_sigma_m = error_sigma_mm / 1000.0
error_low_m   = error_low_mm   / 1000.0
error_high_m  = error_high_mm  / 1000.0

# ------------------ ВСПОМОГАТЕЛЬНОЕ ------------------

def clipped_gauss(mu: float, sigma: float, low: float, high: float) -> float:
    """Гауссов шум/биас с обрезанием по диапазону."""
    if sigma <= 0:
        return max(low, min(high, mu))
    while True:
        val = random.gauss(mu, sigma)
        if low <= val <= high:
            return val

# ------------------ ЭМУЛЯТОР ОСИ ------------------

class AxisEmulator:
    def __init__(self):
        self.nominal = start_m
        self.dt = 1.0 / hz

    def _emit(self, nominal_m: float, error_bias_m: float | None = None):
        noise = clipped_gauss(0.0, noise_sigma_m, noise_low_m, noise_high_m)
        distance = nominal_m + (error_bias_m or 0.0) + noise
        print(json.dumps({"distance": round(distance, 12)}), flush=True)

    def _run_for_duration(self, seconds: float, error_bias_m: float | None = None):
        end_t = time.perf_counter() + seconds
        next_tick = time.perf_counter()
        while True:
            if time.perf_counter() >= end_t:
                break
            self._emit(self.nominal, error_bias_m)
            next_tick += self.dt
            sleep_s = next_tick - time.perf_counter()
            if sleep_s > 0:
                time.sleep(sleep_s)

    def _move_to(self, target_m: float):
        direction = 1.0 if target_m >= self.nominal else -1.0
        v = feed_m_per_sec * direction
        next_tick = time.perf_counter()
        while True:
            # достигли цели?
            if (direction > 0 and self.nominal >= target_m) or (direction < 0 and self.nominal <= target_m):
                self.nominal = target_m
                self._emit(self.nominal)
                break

            # движемся с заданной подачей и эмитим данные по тактам
            self._emit(self.nominal)
            self.nominal += v * self.dt
            # защита от перелёта
            if (direction > 0 and self.nominal > target_m) or (direction < 0 and self.nominal < target_m):
                self.nominal = target_m

            next_tick += self.dt
            sleep_s = next_tick - time.perf_counter()
            if sleep_s > 0:
                time.sleep(sleep_s)

    def run(self):
        # формируем точки ВПЕРЁД: start+step ... end
        points = []
        x = start_m + step_m
        while x <= end_m + 1e-15:
            points.append(x)
            x += step_m

        # ---------- ПРОХОД ВПЕРЁД ----------
        # На каждой точке: подходы "точка ↔ (точка - шаг)"
        for point_m in points:
            for cyc in range(measurements_per_step):
                # подъезд к точке
                self._move_to(point_m)
                # стоянка на точке
                error_bias = clipped_gauss(0.0, error_sigma_m, error_low_m, error_high_m)
                self._run_for_duration(wait_at_point_s, error_bias)
                # если не последний подход — откат ровно на один шаг ВЛЕВО
                if cyc < measurements_per_step - 1:
                    back_left = max(start_m, point_m - step_m)
                    self._move_to(back_left)
                    error_bias_back = clipped_gauss(0.0, error_sigma_m, error_low_m, error_high_m)
                    self._run_for_duration(wait_back_s, error_bias_back)

        # ---------- РАЗВОРОТ ----------
        # Уходим ещё на один шаг вправо: end + step (например, 120 мм)
        extra_right = end_m + step_m
        self._move_to(extra_right)
        # короткая пауза, чтобы автоматика наверняка увидела выход из зоны
        self._run_for_duration(wait_back_s, clipped_gauss(0.0, error_sigma_m, error_low_m, error_high_m))

        # ---------- ПРОХОД НАЗАД ----------
        # На каждой точке: подходы "точка ↔ ПРАВЫЙ сосед"
        # Для верхней точки правый сосед = extra_right,
        # далее правый сосед = предыдущая (более правая) точка в обратном списке.
        reversed_points = list(reversed(points))
        for i, point_m in enumerate(reversed_points):
            right_neighbor = extra_right if i == 0 else reversed_points[i-1]
            for cyc in range(measurements_per_step):
                # подъезд к текущей рабочей точке (100, затем 80, 60, ...)
                self._move_to(point_m)
                error_bias = clipped_gauss(0.0, error_sigma_m, error_low_m, error_high_m)
                self._run_for_duration(wait_at_point_s, error_bias)
                # если не последний подход — откат к ПРАВОМУ соседу
                if cyc < measurements_per_step - 1:
                    self._move_to(right_neighbor)
                    error_bias_back = clipped_gauss(0.0, error_sigma_m, error_low_m, error_high_m)
                    self._run_for_duration(wait_back_s, error_bias_back)

# ------------------ ЗАПУСК ------------------

def main():
    emu = AxisEmulator()
    emu.run()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
