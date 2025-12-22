#ifndef CONFIG_H
#define CONFIG_H

// I2C settings
#define I2C_SDA_PIN 21
#define I2C_SCK_PIN 22

// I2C Addresses
#define MCP23017_INPUT_ADDR   0x21  // U8 MCP23017 (Digital Inputs) - Address 0x21 (Default JP1 Short - 0x21)
#define MCP23017_OUTPUT_ADDR  0x20  // U26 MCP23017 (Relay Outputs) - Address 0x20 (Default Open Jumpers - 0x20)
#define GP8413_DAC_ADDR      0x58  // U46 GP8413 DAC Address - Default 0x58

// Digital Input Pins (ESP32)
#define PIN_RESET_BUTTON     0   // EN pin
#define PIN_BOOT_ENABLE      0   // GPIO0 - BOOT Enable
#define PIN_BUZZER           2   // GPIO2 - Buzzer (BEEP)

// MCP23017 Input Interrupt Pins
#define PIN_MCP_INTA        14   // GPIO14 - U8 MCP23017 PORT A external Interrupt
#define PIN_MCP_INTB        13   // GPIO13 - U8 MCP23017 PORT B external Interrupt (8 Digital Inputs)

// Analog Input Pins
#define PIN_ANALOG_CH1      36   // GPIO36 - 0-5V Analog Input Channel 1
#define PIN_ANALOG_CH2      39   // GPIO39 - 0-5V Analog Input Channel 2
#define PIN_CURRENT_CH1     34   // GPIO34 - 4-20mA Analog Input Channel 1
#define PIN_CURRENT_CH2     35   // GPIO35 - 4-20mA Analog Input Channel 2

// DHT22 Temperature/Humidity Sensor Pins
#define PIN_DHT_SENSOR1      4   // GPIO4 - DHT22 (AM2302) Temperature/Humidity Sensor Input Channel 1
#define PIN_DHT_SENSOR2     15   // GPIO15 - DHT22 (AM2302) Temperature/Humidity Sensor Input Channel 2

// DS18B20 Temperature Sensor Pin
#define PIN_DS18B20         12   // GPIO12 - DS18B20 OneWire Temperature Sensors

// RS485 MODBUS Pins
#define PIN_MAX485_RO       16   // GPIO16 - RS485 MODBUS MAX485 RO pin (RXD)
#define PIN_MAX485_DI       17   // GPIO17 - RS485 MODBUS MAX485 DI pin (TXD)
#define PIN_MAX485_TXRX     27   // GPIO27 - MAX485 TXRX Control pin for MODBUS (RS485)

// RF433 Pins
#define PIN_RF433_TX        32   // GPIO32 - 433MHz RF Transmitter TX
#define PIN_RF433_RX        33   // GPIO33 - 433MHz RF Receiver RX

// GSM Module Pins
#define PIN_GSM_TX          25   // GPIO25 - GSM SIM800L/SIM7600E TX
#define PIN_GSM_RX          26   // GPIO26 - GSM SIM800L/SIM7600E RX

// Ethernet W5500 MODULE SPI Pins
#define PIN_ETH_CS           5   // GPIO5 - ETHERNET W5500 MODULE SPI Chip Select Pin
#define PIN_ETH_SCLK        18   // GPIO18 - ETHERNET W5500 MODULE SPI SCLK
#define PIN_ETH_MISO        19   // GPIO19 - ETHERNET W5500 MODULE SPI MISO
#define PIN_ETH_MOSI        23   // GPIO23 - ETHERNET W5500 MODULE SPI MOSI

// MCP23017 pins for Ethernet and RTC
#define MCP_ETH_RESET_PORT  MCP23017Port::A  // U8 INPUT MCP23017 port used for W5500 reset
#define MCP_ETH_RESET_PIN   5                // GPA5 pin for W5500 reset (on INPUT MCP23017)
#define MCP_RTC_SQW_PORT    MCP23017Port::A  // U8 INPUT MCP23017 port used for RTC SQW
#define MCP_RTC_SQW_PIN     7                // GPA7 pin for RTC SQW (on INPUT MCP23017)

// Ethernet settings - Enhanced for better W5500 initialization
#define ETH_RESET_DURATION       50     // Reset pulse duration in ms
#define ETH_RECOVERY_DELAY       200    // Time to wait after reset release
#define ETH_INIT_TIMEOUT         10000  // Timeout for Ethernet initialization in ms
#define ETH_SPI_FREQ             4000000// Reduced SPI frequency for better stability

// ADC Parameters
#define ADC_RESOLUTION      4095    // 12-bit ADC
#define ADC_VOLTAGE_REF     3.3     // Reference voltage in Volts
#define CURRENT_LOOP_RESISTOR 165.0 // 165 ohm for 4-20mA to voltage conversion

// Constants
#define NUM_DIGITAL_INPUTS   8
#define NUM_RELAY_OUTPUTS    6
#define NUM_ANALOG_CHANNELS  2
#define NUM_CURRENT_CHANNELS 2
#define NUM_DAC_CHANNELS	 2
#define NUM_DHT_SENSORS      2
#define MAX_DS18B20_SENSORS  8      // Maximum number of DS18B20 sensors

// Modbus register addresses
#define MB_REG_INPUTS_START     0
#define MB_REG_RELAYS_START    10
#define MB_REG_ANALOG_START    20
#define MB_REG_TEMP_START      30
#define MB_REG_HUM_START       40
#define MB_REG_DS18B20_START   50
#define MB_REG_DAC_START       70
#define MB_REG_RTC_START       80

#endif // CONFIG_H