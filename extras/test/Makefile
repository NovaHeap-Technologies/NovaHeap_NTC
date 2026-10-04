CXX ?= g++
CXXFLAGS = -std=c++11 -Wall -Wextra -Werror -I. -I../../src
SOURCES = test_ntc.cpp ../../src/NovaHeap_NTC.cpp
DEPS = $(SOURCES) ../../src/NovaHeap_NTC.h Arduino.h

# The same tests, built three times: as a generic board, and with ESP32's and ESP8266's
# board defaults.
test: test_ntc test_ntc_esp32 test_ntc_esp8266
	./test_ntc
	./test_ntc_esp32
	./test_ntc_esp8266

test_ntc: $(DEPS)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES)

test_ntc_esp32: $(DEPS)
	$(CXX) $(CXXFLAGS) -DARDUINO_ARCH_ESP32 -o $@ $(SOURCES)

test_ntc_esp8266: $(DEPS)
	$(CXX) $(CXXFLAGS) -DARDUINO_ARCH_ESP8266 -o $@ $(SOURCES)

clean:
	rm -f test_ntc test_ntc_esp32 test_ntc_esp8266 *.exe

.PHONY: test clean
