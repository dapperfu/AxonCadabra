.PHONY: help clean build run \
	android-clean android-build \
	esp32-clean esp32-build esp32-flash esp32-monitor \
	format

PIO_DIR := firmware/esp32

help:
	@echo "Targets:"
	@echo "  clean          Clean Android + ESP32 outputs"
	@echo "  build          Build Android + ESP32 outputs"
	@echo "  run            Convenience alias for esp32-monitor"
	@echo ""
	@echo "Android:"
	@echo "  android-clean  Run Gradle clean"
	@echo "  android-build  Build debug APK"
	@echo ""
	@echo "ESP32 (PlatformIO):"
	@echo "  esp32-clean    pio clean"
	@echo "  esp32-build    pio build"
	@echo "  esp32-flash    pio upload"
	@echo "  esp32-monitor  pio device monitor"
	@echo ""
	@echo "Formatting:"
	@echo "  format         Run clang-format over C sources"

clean: android-clean esp32-clean

build: android-build esp32-build

run: esp32-monitor

android-clean:
	./gradlew clean

android-build:
	./gradlew assembleDebug

esp32-clean:
	rm -f $(PIO_DIR)/sdkconfig.*
	pio run -d $(PIO_DIR) -t clean

esp32-build:
	pio run -d $(PIO_DIR)

esp32-flash:
	pio run -d $(PIO_DIR) -t upload

esp32-monitor:
	pio device monitor -d $(PIO_DIR)

format:
	clang-format -i $$(find firmware -type f \\( -name '*.c' -o -name '*.h' \\) 2>/dev/null)
