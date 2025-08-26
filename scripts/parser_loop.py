import smaract.si as si
import time
import json

print(">>> parser_loop started", flush=True)
# Настройки PicoScale
locator = "usb:ix:0"                     # Идентификатор PicoScale
channel = 0                              # Канал, где лежит позиция
source = 0                               # Номер источника (SRC0)

# Подключение к устройству
handle = si.Open(locator)

try:
    while True:
        # Считываем расстояние
        value = si.GetValue_f64(handle, channel, source)
        # Выводим как JSON в stdout
        print(json.dumps({"distance": value}), flush=True)
        time.sleep(1)

except KeyboardInterrupt:
    pass
finally:
    si.Close(handle)
