import sys
import json
import time
import random

# Настройки генерации
mean = 0.055          # Среднее значение
stddev = 0.018        # Стандартное отклонение
lower = 0.01
upper = 0.099999
interval = 0.2        # Период вывода (сек)

def clipped_gauss(mu, sigma, low, high):
    """Генерирует значение с нормальным распределением и ограничением по диапазону."""
    while True:
        val = random.gauss(mu, sigma)
        if low <= val <= high:
            return val

try:
    while True:
        value = clipped_gauss(mean, stddev, lower, upper)
        data = {"distance": round(value, 6)}
        print(json.dumps(data), flush=True)
        time.sleep(interval)
except KeyboardInterrupt:
    pass
