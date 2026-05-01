Import("env")

import sys


def block_hardware_upload(source, target, env):
    print("")
    print("=" * 60)
    print("ERROR: refusing to upload the 'wokwi' env to real hardware.")
    print("")
    print("The 'wokwi' env builds only main_wokwi.cpp and strips out")
    print("WiFi, ESP-NOW, HX711 and LittleFS. Flashing it to a real")
    print("ESP32 will brick the device's normal behaviour.")
    print("")
    print("To upload to real hardware run:")
    print("    pio run -e esp32dev -t upload")
    print("=" * 60)
    print("")
    sys.exit(1)


env.AddPreAction("upload", block_hardware_upload)
env.AddPreAction("uploadfs", block_hardware_upload)
env.AddPreAction("uploadfsota", block_hardware_upload)
