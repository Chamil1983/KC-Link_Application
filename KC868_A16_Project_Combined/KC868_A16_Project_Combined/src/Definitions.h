#pragma once
/**
 * Definitions.h
 * Shared includes + compile-time constants for KC868-A16 modular firmware.
 * Auto-generated from original KC868_A16_Controller.ino
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Preferences.h>
#include <time.h>
#include <FS.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <RCSwitch.h>
#include <RTClib.h>
#include <DNSServer.h>
#include <PCF8574.h>  // Using Renzo Mischianti's library
#include <esp_intr_alloc.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

#define PCF8574_INPUTS_1_8    0x22
#define PCF8574_INPUTS_9_16   0x21
#define PCF8574_OUTPUTS_1_8   0x24
#define PCF8574_OUTPUTS_9_16  0x25
#define SDA_PIN               4
#define SCL_PIN               5
#define HT1_PIN               32
#define HT2_PIN               33
#define HT3_PIN               14
#define RF_RX_PIN             2
#define RF_TX_PIN             15
#define RS485_TX_PIN          13
#define RS485_RX_PIN          16
#define ANALOG_PIN_1          36
#define ANALOG_PIN_2          34
#define ANALOG_PIN_3          35
#define ANALOG_PIN_4          39
#define ADC_MAX_VALUE         4095    // ESP32 ADC is 12-bit (0-4095)
#define ADC_VOLTAGE_MAX       3.3     // ESP32 ADC reference voltage is 3.3V
#define ANALOG_VOLTAGE_MAX    5.0     // Full scale of the analog inputs is 5V
#define ETH_PHY_ADDR          0
#define ETH_PHY_MDC           23
#define ETH_PHY_MDIO          18
#define ETH_PHY_POWER         -1
#define ETH_PHY_TYPE          ETH_PHY_LAN8720
#define ETH_CLK_MODE          ETH_CLOCK_GPIO17_OUT
#define EEPROM_SIZE           4096
#define EEPROM_WIFI_SSID_ADDR 0
#define EEPROM_WIFI_PASS_ADDR 64
#define EEPROM_NETCFG_ADDR    128  // reserved (128 bytes) for network settings backup
#define EEPROM_NETCFG_MAGIC   0x4E434647UL // 'NCFG'
#define EEPROM_CONFIG_ADDR    256
#define EEPROM_COMM_ADDR      384
#define EEPROM_SCHEDULE_ADDR  512
#define EEPROM_TRIGGER_ADDR   2048
#define EEPROM_COMM_CONFIG_ADDR 3072
#define EEPROM_INTERRUPT_CONFIG_ADDR 3584
#define MAX_SCHEDULES         30
#define MAX_ANALOG_TRIGGERS   16
#define MAX_INTERRUPT_HANDLERS 16
#define INPUT_PRIORITY_HIGH 1
#define INPUT_PRIORITY_MEDIUM 2
#define INPUT_PRIORITY_LOW 3
#define INPUT_PRIORITY_NONE 0
#define INTERRUPT_TRIGGER_RISING     0
#define INTERRUPT_TRIGGER_FALLING    1
#define INTERRUPT_TRIGGER_CHANGE     2
#define INTERRUPT_TRIGGER_HIGH_LEVEL 3
#define INTERRUPT_TRIGGER_LOW_LEVEL  4
#define SENSOR_TYPE_DIGITAL  0  // General digital input
#define SENSOR_TYPE_DHT11    1  // DHT11 temperature/humidity sensor
#define SENSOR_TYPE_DHT22    2  // DHT22/AM2302 temperature/humidity sensor
#define SENSOR_TYPE_DS18B20  3  // DS18B20 temperature sensor
#define FIRMWARE_VERSION firmwareVersion

// -----------------------------------------------------------------------------
// Custom MAC assignments (requested)
// -----------------------------------------------------------------------------
// Ethernet MAC is already enforced in NetworkManager.cpp (ETHERNET_MAC).
// Add explicit WiFi Station/AP MACs here for a single source of truth.
#ifndef WIFI_STA_MAC
#define WIFI_STA_MAC "C8:2E:A3:F5:7D:DB"
#endif

#ifndef WIFI_AP_MAC
#define WIFI_AP_MAC  "C8:2E:A3:F5:7D:DC"
#endif

// Keep Ethernet MAC identical to the proven LAN8720.ino test sketch.
//#ifndef ETHERNET_MAC
#define ETHERNET_MAC "C8:2E:A3:F5:7D:DA"
//#endif

