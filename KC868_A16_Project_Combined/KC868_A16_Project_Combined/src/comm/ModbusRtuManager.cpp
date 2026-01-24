// ModbusRtuManager.cpp
#include "ModbusRtuManager.h"
#include "../FunctionPrototypes.h"
#include <ModbusRTU.h>
#include <Preferences.h>
#include <math.h>

static ModbusRTU mb;

// ---- Constants (register map) ----
static const uint16_t MAP_VERSION = 0x0100;
static const uint16_t MODEL_ID = 0xA016;
static const uint16_t YEAR_DEV = 2026;

// Holding register ranges (0-based offsets)
static const uint16_t HR_MAP_VERSION = 0;      // 40001
static const uint16_t HR_MODEL_ID = 1;         // 40002
static const uint16_t HR_FW_PACKED = 2;        // 40003
static const uint16_t HR_HW_PACKED = 3;        // 40004
static const uint16_t HR_YEAR_DEV = 4;         // 40005
static const uint16_t HR_CAPS = 5;             // 40006

static const uint16_t HR_BOARDNAME_START = 10; // 40011 (16 regs)
static const uint16_t HR_SERIAL_START = 26;    // 40027 (16 regs)
static const uint16_t HR_MANUF_START = 42;     // 40043 (16 regs, RO)
static const uint16_t HR_FW_STR_START = 58;    // 40059 (8 regs)
static const uint16_t HR_HW_STR_START = 66;    // 40067 (8 regs)
static const uint16_t HR_DEVNAME_START = 74;   // 40075 (16 regs)

static const uint16_t HR_EFUSE_MAC_START = 100; // 40101 (6 regs bytes)
static const uint16_t HR_ETH_MAC_START   = 106; // 40107
static const uint16_t HR_WIFI_STA_MAC_START = 112; // 40113
static const uint16_t HR_WIFI_AP_MAC_START  = 118; // 40119

static const uint16_t HR_RS485_PROTOCOL = 200;   // 40201
static const uint16_t HR_MB_SLAVE_ID    = 201;   // 40202
static const uint16_t HR_MB_BAUD        = 202;   // 40203
static const uint16_t HR_MB_DATABITS    = 203;   // 40204
static const uint16_t HR_MB_PARITY      = 204;   // 40205
static const uint16_t HR_MB_STOPBITS    = 205;   // 40206
static const uint16_t HR_MB_FRAMEGAP_MS = 206;   // 40207
static const uint16_t HR_MB_TIMEOUT_MS  = 207;   // 40208
static const uint16_t HR_MB_APPLY_SAVE  = 208;   // 40209 (W)
static const uint16_t HR_MB_APPLY_STATUS = 209;  // 40210 (R)

static const uint16_t HR_USB_BAUD       = 210;   // 40211
static const uint16_t HR_USB_DATABITS   = 211;   // 40212
static const uint16_t HR_USB_PARITY     = 212;   // 40213
static const uint16_t HR_USB_STOPBITS   = 213;   // 40214

static const uint16_t HR_OUTMASK_WRITE  = 300;   // 40301
static const uint16_t HR_OUTMASK_APPLY  = 301;   // 40302
static const uint16_t HR_OUTMASK_CUR    = 302;   // 40303 (RO mirror)
static const uint16_t HR_INMASK_CUR     = 303;   // 40304 (RO mirror)
static const uint16_t HR_ANALOG_SCALE_START = 310; // 40311.. (8 regs = 4 floats)
static const uint16_t HR_ANALOG_OFFSET_START = 318; // 40319.. (8 regs = 4 floats)

static const uint16_t HR_CMD_ARM        = 609;   // 40610
static const uint16_t HR_CMD_CODE       = 610;   // 40611
static const uint16_t HR_CMD_ARG0       = 611;   // 40612
static const uint16_t HR_CMD_ARG1       = 612;   // 40613
static const uint16_t HR_CMD_CONFIRM    = 613;   // 40614
static const uint16_t HR_CMD_STATUS     = 614;   // 40615
static const uint16_t HR_CMD_RESULT     = 615;   // 40616
static const uint16_t HR_CMD_LASTERR    = 616;   // 40617

// Input register offsets (0-based)
static const uint16_t IR_MAP_VERSION = 0;     // 30001
static const uint16_t IR_DEVICE_STATUS = 1;   // 30002
static const uint16_t IR_HEARTBEAT = 2;       // 30003
static const uint16_t IR_UPTIME_LO = 3;       // 30004
static const uint16_t IR_UPTIME_HI = 4;       // 30005
static const uint16_t IR_CHANGE_SEQ = 5;      // 30006
static const uint16_t IR_LAST_ERROR = 8;      // 30009
static const uint16_t IR_FREE_HEAP_LO = 9;    // 30010
static const uint16_t IR_FREE_HEAP_HI = 10;   // 30011
static const uint16_t IR_CPU_FREQ = 11;       // 30012
static const uint16_t IR_AI_RAW_START = 12;   // 30013..16
static const uint16_t IR_AI_MV_START = 16;    // 30017..20
static const uint16_t IR_DHT1_T = 20;         // 30021
static const uint16_t IR_DHT1_RH = 21;        // 30022
static const uint16_t IR_DHT2_T = 22;         // 30023
static const uint16_t IR_DHT2_RH = 23;        // 30024
static const uint16_t IR_DS18_T = 24;         // 30025
static const uint16_t IR_SENSOR_STATUS = 25;  // 30026
static const uint16_t IR_RTC_UNIX_LO = 26;    // 30027
static const uint16_t IR_RTC_UNIX_HI = 27;    // 30028
static const uint16_t IR_RTC_YEAR = 28;       // 30029
static const uint16_t IR_RTC_MONTH = 29;      // 30030
static const uint16_t IR_RTC_DAY = 30;        // 30031
static const uint16_t IR_RTC_HOUR = 31;       // 30032
static const uint16_t IR_RTC_MIN = 32;        // 30033
static const uint16_t IR_RTC_SEC = 33;        // 30034
static const uint16_t IR_OUTMASK = 39;        // 30040
static const uint16_t IR_INMASK = 40;         // 30041
static const uint16_t IR_DIRECTMASK = 41;     // 30042
static const uint16_t IR_SYSFLAGS = 42;       // 30043

// Coils (0-based)
static const uint16_t COIL_DO_START = 0;      // 00001..00016
static const uint16_t COIL_MASTER_ENABLE = 18; // 00019
static const uint16_t COIL_NIGHT_MODE = 19;    // 00020

// Discrete inputs (0-based)
static const uint16_t DI_MAIN_START = 0;      // 10001..10016
static const uint16_t DI_DIRECT_START = 16;   // 10017..10019
static const uint16_t DI_ETH_CONNECTED = 19;  // 10020
static const uint16_t DI_WIFI_CONNECTED = 20; // 10021
static const uint16_t DI_AP_MODE = 21;        // 10022
static const uint16_t DI_RESTART_REQUIRED = 22; // 10023
static const uint16_t DI_MODBUS_ACTIVE = 23;    // 10024

// Safe command tokens
static const uint16_t CMD_ARM_TOKEN = 0xA55A;
static const uint16_t CMD_CONFIRM_TOKEN = 0x5AA5;

// Command codes
static const uint16_t CMD_REBOOT = 1;
static const uint16_t CMD_SAVE_CONFIG = 2;
static const uint16_t CMD_RELOAD_CONFIG = 3;
static const uint16_t CMD_FACTORY_DEFAULTS = 4;

static bool g_running = false;
static uint32_t g_lastHeartbeatMs = 0;
static uint16_t g_changeSeq = 0;
static uint16_t g_lastOutMask = 0xFFFF;
static uint16_t g_lastInMask = 0xFFFF;
static uint16_t g_lastDirectMask = 0xFFFF;
static uint16_t g_lastSysFlags = 0xFFFF;
static bool g_lastMasterEnable = true;
static bool g_lastNightMode = false;


// Snapshot of DO coil image to detect external Modbus writes vs local/web changes
static bool g_lastDoCoils[16] = { false };
// For detecting holding-reg edits
static uint16_t lastApplySaveValue = 0;
static uint16_t lastCmdArm = 0;
static uint16_t lastCmdConfirm = 0;
static uint16_t lastOutmaskWrite = 0;
static uint16_t lastOutmaskApply = 0;

// Helpers: pack/unpack
static uint16_t packVersionMMmm(const String& v) {
    // Parse "M.m.p" style; pack MMmm as (major<<8) | minor
    int major = 0, minor = 0;
    int dot = v.indexOf('.');
    if (dot >= 0) {
        major = v.substring(0, dot).toInt();
        int dot2 = v.indexOf('.', dot + 1);
        if (dot2 >= 0) minor = v.substring(dot + 1, dot2).toInt();
        else minor = v.substring(dot + 1).toInt();
    } else {
        major = v.toInt();
    }
    major = constrain(major, 0, 255);
    minor = constrain(minor, 0, 255);
    return (uint16_t)((major << 8) | minor);
}

static void setU32Ireg(uint16_t base, uint32_t v) {
    mb.Ireg(base, (uint16_t)(v & 0xFFFF));
    mb.Ireg(base + 1, (uint16_t)((v >> 16) & 0xFFFF));
}

static void setU32Hreg(uint16_t base, uint32_t v) {
    mb.Hreg(base, (uint16_t)(v & 0xFFFF));
    mb.Hreg(base + 1, (uint16_t)((v >> 16) & 0xFFFF));
}

static uint32_t getU32Hreg(uint16_t base) {
    uint32_t lo = (uint32_t)mb.Hreg(base);
    uint32_t hi = (uint32_t)mb.Hreg(base + 1);
    return lo | (hi << 16);
}

static void setFloatHreg(uint16_t base, float f) {
    union { float f; uint32_t u; } u;
    u.f = f;
    mb.Hreg(base, (uint16_t)(u.u & 0xFFFF));
    mb.Hreg(base + 1, (uint16_t)((u.u >> 16) & 0xFFFF));
}

static float getFloatHreg(uint16_t base) {
    union { float f; uint32_t u; } u;
    uint32_t lo = (uint32_t)mb.Hreg(base);
    uint32_t hi = (uint32_t)mb.Hreg(base + 1);
    u.u = lo | (hi << 16);
    return u.f;
}

static void writeAsciiStringToHreg(uint16_t start, uint16_t regCount, const String& s) {
    // 2 chars per reg, high byte then low byte
    uint16_t totalChars = regCount * 2;
    for (uint16_t i = 0; i < regCount; i++) {
        uint8_t c1 = 0, c2 = 0;
        uint16_t charIndex = i * 2;
        if (charIndex < s.length()) c1 = (uint8_t)s[charIndex];
        if (charIndex + 1 < s.length()) c2 = (uint8_t)s[charIndex + 1];
        mb.Hreg(start + i, (uint16_t)((c1 << 8) | c2));
    }
}

static String readAsciiStringFromHreg(uint16_t start, uint16_t regCount) {
    String out;
    out.reserve(regCount * 2);
    for (uint16_t i = 0; i < regCount; i++) {
        uint16_t w = mb.Hreg(start + i);
        char c1 = (char)((w >> 8) & 0xFF);
        char c2 = (char)(w & 0xFF);
        if (c1 == 0) break;
        out += c1;
        if (c2 == 0) break;
        out += c2;
    }
    out.trim();
    return out;
}

static bool parseMacBytes(const String& macStr, uint8_t out[6]) {
    int vals[6];
    if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x",
        &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)vals[i];
    return true;
}

static String macBytesToString(const uint8_t macb[6]) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
        macb[0], macb[1], macb[2], macb[3], macb[4], macb[5]);
    return String(buf);
}

static void ensureSerialNumber() {
    if (serialNumber.length() > 0) return;
    uint64_t ef = ESP.getEfuseMac();
    uint8_t macb[6];
    macb[0] = (uint8_t)(ef >> 40);
    macb[1] = (uint8_t)(ef >> 32);
    macb[2] = (uint8_t)(ef >> 24);
    macb[3] = (uint8_t)(ef >> 16);
    macb[4] = (uint8_t)(ef >> 8);
    macb[5] = (uint8_t)(ef);
    serialNumber = "KC868-" + String(macb[3], HEX) + String(macb[4], HEX) + String(macb[5], HEX);
    serialNumber.toUpperCase();
}

static uint16_t computeCapabilities() {
    uint16_t caps = 0;
    // bit0=Ethernet, bit1=WiFi, bit2=WebServer, bit3=WebSocket
    caps |= 1 << 0;
    caps |= 1 << 1;
    caps |= 1 << 2;
    caps |= 1 << 3;
    // bit4=RS485, bit5=ModbusRTU
    caps |= 1 << 4;
    caps |= 1 << 5;
    // bit6=RTC, bit7=Sensors, bit8=Analog, bit9=Schedules/Triggers
    caps |= 1 << 6;
    caps |= 1 << 7;
    caps |= 1 << 8;
    caps |= 1 << 9;
    return caps;
}

static uint16_t buildOutMaskFromState() {
    uint16_t m = 0;
    for (int i = 0; i < 16; i++) if (outputStates[i]) m |= (1u << i);
    return m;
}

static uint16_t buildInMaskFromState() {
    uint16_t m = 0;
    for (int i = 0; i < 16; i++) if (inputStates[i]) m |= (1u << i);
    return m;
}

static uint16_t buildDirectMaskFromState() {
    uint16_t m = 0;
    for (int i = 0; i < 3; i++) if (directInputStates[i]) m |= (1u << i);
    return m;
}

static uint16_t buildSysFlags() {
    uint16_t f = 0;
    if (ethConnected) f |= (1u << 0);
    if (WiFi.status() == WL_CONNECTED) f |= (1u << 1);
    if (apMode) f |= (1u << 2);
    if (outputsMasterEnable) f |= (1u << 3);
    if (restartRequired) f |= (1u << 4);
    if (modbusRtuActive) f |= (1u << 5);
    return f;
}

static void applyOutputsFromCoils() {
    if (!outputsMasterEnable) {
        // Force all outputs OFF
        bool any = false;
        for (int i = 0; i < 16; i++) {
            if (outputStates[i]) { outputStates[i] = false; any = true; }
        }
        if (any) writeOutputs();
        return;
    }

    bool changed = false;
    for (int i = 0; i < 16; i++) {
        bool desired = mb.Coil(COIL_DO_START + i);
        g_lastDoCoils[i] = desired;
        if (outputStates[i] != desired) {
            outputStates[i] = desired;
            changed = true;
        }
    }
    if (changed) writeOutputs();
}

static void syncCoilsToOutputs() {
    // Keep coil image reflecting desired/actual output states.
    for (int i = 0; i < 16; i++) {
        mb.Coil(COIL_DO_START + i, outputStates[i]);
        g_lastDoCoils[i] = outputStates[i];
    }
}

static void applyOutmaskWriteIfNeeded() {
    uint16_t w = mb.Hreg(HR_OUTMASK_WRITE);
    uint16_t m = mb.Hreg(HR_OUTMASK_APPLY);

    if (w == lastOutmaskWrite && m == lastOutmaskApply) return;

    lastOutmaskWrite = w;
    lastOutmaskApply = m;

    // Apply mask bits
    if (!outputsMasterEnable) return;
    bool changed = false;
    for (int i = 0; i < 16; i++) {
        if ((m >> i) & 1u) {
            bool desired = ((w >> i) & 1u) != 0;
            if (outputStates[i] != desired) {
                outputStates[i] = desired;
                changed = true;
            }
        }
    }
    if (changed) {
        writeOutputs();
        syncCoilsToOutputs();
    }
}

static void handleApplySaveSerialSettings() {
    uint16_t v = mb.Hreg(HR_MB_APPLY_SAVE);
    if (v == lastApplySaveValue) return;
    lastApplySaveValue = v;

    if (v != CMD_ARM_TOKEN) {
        // Only act on exact token; ignore other writes
        return;
    }

    // Copy HRs into globals
    int proto = (int)mb.Hreg(HR_RS485_PROTOCOL);
    rs485Protocol = (proto == 1) ? "Modbus RTU" : "Custom";
    rs485DeviceAddress = (int)mb.Hreg(HR_MB_SLAVE_ID);
    rs485BaudRate = (int)mb.Hreg(HR_MB_BAUD);
    rs485DataBits = (int)mb.Hreg(HR_MB_DATABITS);
    rs485Parity = (int)mb.Hreg(HR_MB_PARITY);
    rs485StopBits = (int)mb.Hreg(HR_MB_STOPBITS);
    rs485FrameGapMs = (uint16_t)mb.Hreg(HR_MB_FRAMEGAP_MS);
    rs485TimeoutMs = (uint16_t)mb.Hreg(HR_MB_TIMEOUT_MS);

    usbBaudRate = (int)mb.Hreg(HR_USB_BAUD);
    usbDataBits = (int)mb.Hreg(HR_USB_DATABITS);
    usbParity = (int)mb.Hreg(HR_USB_PARITY);
    usbStopBits = (int)mb.Hreg(HR_USB_STOPBITS);

    // Persist
    saveCommunicationConfig();
    saveConfiguration();

    restartRequired = true;
    mb.Hreg(HR_MB_APPLY_STATUS, 1); // ok
    // Clear write register to avoid re-trigger
    mb.Hreg(HR_MB_APPLY_SAVE, 0);
}

static void handleRwStrings() {
    // Board Name
    String bn = readAsciiStringFromHreg(HR_BOARDNAME_START, 16);
    if (bn.length() > 0 && bn != boardName) boardName = bn;

    // Serial
    String sn = readAsciiStringFromHreg(HR_SERIAL_START, 16);
    if (sn.length() > 0 && sn != serialNumber) serialNumber = sn;

    // Device Name
    String dn = readAsciiStringFromHreg(HR_DEVNAME_START, 16);
    if (dn.length() > 0 && dn != deviceName) deviceName = dn;
}

static void handleAnalogCalibrationRw() {
    for (int ch = 0; ch < 4; ch++) {
        float sc = getFloatHreg(HR_ANALOG_SCALE_START + ch * 2);
        float of = getFloatHreg(HR_ANALOG_OFFSET_START + ch * 2);
        if (isnan(sc) || isinf(sc)) sc = 1.0f;
        if (isnan(of) || isinf(of)) of = 0.0f;
        analogScaleFactors[ch] = sc;
        analogOffsetValues[ch] = of;
    }
}

static void handleMacRw() {
    // Read configured MAC bytes back into strings (writeable regs).
    uint8_t macb[6];

    for (int i = 0; i < 6; i++) macb[i] = (uint8_t)mb.Hreg(HR_ETH_MAC_START + i);
    String newEth = macBytesToString(macb);
    if (newEth.length() > 0 && newEth != ethMacConfig) {
        ethMacConfig = newEth;
        restartRequired = true;
    }

    for (int i = 0; i < 6; i++) macb[i] = (uint8_t)mb.Hreg(HR_WIFI_STA_MAC_START + i);
    String newSta = macBytesToString(macb);
    if (newSta.length() > 0 && newSta != wifiStaMacConfig) {
        wifiStaMacConfig = newSta;
        restartRequired = true;
    }

    for (int i = 0; i < 6; i++) macb[i] = (uint8_t)mb.Hreg(HR_WIFI_AP_MAC_START + i);
    String newAp = macBytesToString(macb);
    if (newAp.length() > 0 && newAp != wifiApMacConfig) {
        wifiApMacConfig = newAp;
        restartRequired = true;
    }
}


static void executeCommand(uint16_t code, uint16_t arg0, uint16_t arg1) {
    (void)arg0; (void)arg1;
    mb.Hreg(HR_CMD_STATUS, 2); // running
    mb.Hreg(HR_CMD_RESULT, 0);
    mb.Hreg(HR_CMD_LASTERR, 0);

    switch (code) {
    case CMD_REBOOT:
        mb.Hreg(HR_CMD_STATUS, 3);
        delay(50);
        ESP.restart();
        break;

    case CMD_SAVE_CONFIG:
        saveConfiguration();
        saveCommunicationConfig();
        saveNetworkSettings();
        mb.Hreg(HR_CMD_STATUS, 3);
        mb.Hreg(HR_CMD_RESULT, 1);
        break;

    case CMD_RELOAD_CONFIG:
        loadConfiguration();
        loadCommunicationConfig();
        loadNetworkSettings();
        restartRequired = true; // safest
        mb.Hreg(HR_CMD_STATUS, 3);
        mb.Hreg(HR_CMD_RESULT, 1);
        break;

    case CMD_FACTORY_DEFAULTS:
        initializeDefaultConfig();
        // Restore Modbus/RS485 defaults
        rs485Protocol = "Modbus RTU";
        rs485DeviceAddress = 1;
        rs485BaudRate = 38400;
        rs485DataBits = 8;
        rs485Parity = 0;
        rs485StopBits = 1;
        rs485FrameGapMs = 6;
        rs485TimeoutMs = 200;
        outputsMasterEnable = true;
        rs485NightMode = false;

        for (int i = 0; i < 4; i++) { analogScaleFactors[i] = 1.0f; analogOffsetValues[i] = 0.0f; }

        saveConfiguration();
        saveCommunicationConfig();
        saveNetworkSettings();
        mb.Hreg(HR_CMD_STATUS, 3);
        mb.Hreg(HR_CMD_RESULT, 1);
        delay(50);
        ESP.restart();
        break;

    default:
        mb.Hreg(HR_CMD_STATUS, 4);
        mb.Hreg(HR_CMD_LASTERR, 1);
        break;
    }
}

static void handleSafeCommands() {
    uint16_t arm = mb.Hreg(HR_CMD_ARM);
    uint16_t confirm = mb.Hreg(HR_CMD_CONFIRM);

    if (arm != lastCmdArm) {
        lastCmdArm = arm;
        if (arm == CMD_ARM_TOKEN) {
            mb.Hreg(HR_CMD_STATUS, 1); // armed
        }
    }

    if (confirm != lastCmdConfirm) {
        lastCmdConfirm = confirm;
        if (confirm == CMD_CONFIRM_TOKEN && mb.Hreg(HR_CMD_STATUS) == 1) {
            uint16_t code = mb.Hreg(HR_CMD_CODE);
            uint16_t a0 = mb.Hreg(HR_CMD_ARG0);
            uint16_t a1 = mb.Hreg(HR_CMD_ARG1);
            // disarm
            mb.Hreg(HR_CMD_STATUS, 0);
            mb.Hreg(HR_CMD_ARM, 0);
            mb.Hreg(HR_CMD_CONFIRM, 0);
            executeCommand(code, a0, a1);
        }
    }
}

static void refreshIdentityHoldingRegs() {
    ensureSerialNumber();

    mb.Hreg(HR_MAP_VERSION, MAP_VERSION);
    mb.Hreg(HR_MODEL_ID, MODEL_ID);
    mb.Hreg(HR_FW_PACKED, packVersionMMmm(firmwareVersion));
    mb.Hreg(HR_HW_PACKED, packVersionMMmm(hardwareVersionStr));
    mb.Hreg(HR_YEAR_DEV, YEAR_DEV);
    mb.Hreg(HR_CAPS, computeCapabilities());

    // Strings
    writeAsciiStringToHreg(HR_BOARDNAME_START, 16, boardName);
    writeAsciiStringToHreg(HR_SERIAL_START, 16, serialNumber);
    writeAsciiStringToHreg(HR_MANUF_START, 16, String("Microcode Engineering"));
    writeAsciiStringToHreg(HR_FW_STR_START, 8, firmwareVersion);
    writeAsciiStringToHreg(HR_HW_STR_START, 8, hardwareVersionStr);
    writeAsciiStringToHreg(HR_DEVNAME_START, 16, deviceName);

    // EFUSE mac bytes
    uint64_t ef = ESP.getEfuseMac();
    uint8_t macb[6];
    macb[0] = (uint8_t)(ef >> 40);
    macb[1] = (uint8_t)(ef >> 32);
    macb[2] = (uint8_t)(ef >> 24);
    macb[3] = (uint8_t)(ef >> 16);
    macb[4] = (uint8_t)(ef >> 8);
    macb[5] = (uint8_t)(ef);
    for (int i = 0; i < 6; i++) mb.Hreg(HR_EFUSE_MAC_START + i, macb[i]);

    // Configured MACs (bytes)
    if (parseMacBytes(ethMacConfig, macb)) for (int i = 0; i < 6; i++) mb.Hreg(HR_ETH_MAC_START + i, macb[i]);
    if (parseMacBytes(wifiStaMacConfig, macb)) for (int i = 0; i < 6; i++) mb.Hreg(HR_WIFI_STA_MAC_START + i, macb[i]);
    if (parseMacBytes(wifiApMacConfig, macb)) for (int i = 0; i < 6; i++) mb.Hreg(HR_WIFI_AP_MAC_START + i, macb[i]);

    // RS485/MODBUS settings
    mb.Hreg(HR_RS485_PROTOCOL, (rs485Protocol.indexOf("Modbus") >= 0) ? 1 : 0);
    mb.Hreg(HR_MB_SLAVE_ID, (uint16_t)rs485DeviceAddress);
    mb.Hreg(HR_MB_BAUD, (uint16_t)rs485BaudRate);
    mb.Hreg(HR_MB_DATABITS, (uint16_t)rs485DataBits);
    mb.Hreg(HR_MB_PARITY, (uint16_t)rs485Parity);
    mb.Hreg(HR_MB_STOPBITS, (uint16_t)rs485StopBits);
    mb.Hreg(HR_MB_FRAMEGAP_MS, rs485FrameGapMs);
    mb.Hreg(HR_MB_TIMEOUT_MS, rs485TimeoutMs);

    mb.Hreg(HR_USB_BAUD, (uint16_t)usbBaudRate);
    mb.Hreg(HR_USB_DATABITS, (uint16_t)usbDataBits);
    mb.Hreg(HR_USB_PARITY, (uint16_t)usbParity);
    mb.Hreg(HR_USB_STOPBITS, (uint16_t)usbStopBits);

    // Analog calibration floats
    for (int ch = 0; ch < 4; ch++) {
        setFloatHreg(HR_ANALOG_SCALE_START + ch * 2, analogScaleFactors[ch]);
        setFloatHreg(HR_ANALOG_OFFSET_START + ch * 2, analogOffsetValues[ch]);
    }
}

static void refreshDiscreteInputs() {
    for (int i = 0; i < 16; i++) mb.Ists(DI_MAIN_START + i, inputStates[i]);
    for (int i = 0; i < 3; i++) mb.Ists(DI_DIRECT_START + i, directInputStates[i]);
    mb.Ists(DI_ETH_CONNECTED, ethConnected);
    mb.Ists(DI_WIFI_CONNECTED, (WiFi.status() == WL_CONNECTED));
    mb.Ists(DI_AP_MODE, apMode);
    mb.Ists(DI_RESTART_REQUIRED, restartRequired);
    mb.Ists(DI_MODBUS_ACTIVE, modbusRtuActive);
}

static void refreshInputRegisters() {
    mb.Ireg(IR_MAP_VERSION, MAP_VERSION);
    mb.Ireg(IR_DEVICE_STATUS, 0); // reserved/legacy
    // heartbeat
    uint32_t now = millis();
    if (now - g_lastHeartbeatMs >= 1000) {
        g_lastHeartbeatMs = now;
        uint16_t hb = mb.Ireg(IR_HEARTBEAT);
        mb.Ireg(IR_HEARTBEAT, (uint16_t)(hb + 1));
    }
    setU32Ireg(IR_UPTIME_LO, (uint32_t)(millis() / 1000UL));

    // Diagnostics
    mb.Ireg(IR_LAST_ERROR, 0);
    setU32Ireg(IR_FREE_HEAP_LO, (uint32_t)ESP.getFreeHeap());
    mb.Ireg(IR_CPU_FREQ, (uint16_t)getCpuFrequencyMhz());

    // AI raw + mv (apply calibration floats to mv)
    for (int i = 0; i < 4; i++) {
        mb.Ireg(IR_AI_RAW_START + i, (uint16_t)analogValues[i]);
        float mv = analogVoltages[i] * 1000.0f;
        mv = mv * analogScaleFactors[i] + analogOffsetValues[i];
        if (mv < 0) mv = 0;
        if (mv > 5000) mv = 5000;
        mb.Ireg(IR_AI_MV_START + i, (uint16_t)(mv + 0.5f));
    }

    // Sensors (HT1/HT2 DHT, HT3 DS18)
    auto dhtToS16x10 = [](float v) -> int16_t {
        if (isnan(v) || isinf(v)) return (int16_t)0;
        return (int16_t)lroundf(v * 10.0f);
    };
    auto rhToU16x10 = [](float v) -> uint16_t {
        if (isnan(v) || isinf(v)) return (uint16_t)0;
        float x = v * 10.0f;
        if (x < 0) x = 0;
        if (x > 1000) x = 1000;
        return (uint16_t)lroundf(x);
    };

    float dht1t = (htSensorConfig[0].sensorType == SENSOR_TYPE_DHT11 || htSensorConfig[0].sensorType == SENSOR_TYPE_DHT22) ? htSensorConfig[0].temperature : NAN;
    float dht1h = (htSensorConfig[0].sensorType == SENSOR_TYPE_DHT11 || htSensorConfig[0].sensorType == SENSOR_TYPE_DHT22) ? htSensorConfig[0].humidity : NAN;
    float dht2t = (htSensorConfig[1].sensorType == SENSOR_TYPE_DHT11 || htSensorConfig[1].sensorType == SENSOR_TYPE_DHT22) ? htSensorConfig[1].temperature : NAN;
    float dht2h = (htSensorConfig[1].sensorType == SENSOR_TYPE_DHT11 || htSensorConfig[1].sensorType == SENSOR_TYPE_DHT22) ? htSensorConfig[1].humidity : NAN;
    float ds18t = (htSensorConfig[2].sensorType == SENSOR_TYPE_DS18B20) ? htSensorConfig[2].temperature : NAN;

    mb.Ireg(IR_DHT1_T, (uint16_t)(int16_t)dhtToS16x10(dht1t));
    mb.Ireg(IR_DHT1_RH, rhToU16x10(dht1h));
    mb.Ireg(IR_DHT2_T, (uint16_t)(int16_t)dhtToS16x10(dht2t));
    mb.Ireg(IR_DHT2_RH, rhToU16x10(dht2h));
    mb.Ireg(IR_DS18_T, (uint16_t)(int16_t)dhtToS16x10(ds18t));

    uint16_t ss = 0;
    if (!isnan(dht1t) && !isnan(dht1h)) ss |= 1u << 0;
    if (!isnan(dht2t) && !isnan(dht2h)) ss |= 1u << 1;
    if (!isnan(ds18t)) ss |= 1u << 2;
    mb.Ireg(IR_SENSOR_STATUS, ss);

    // RTC
    if (rtcInitialized) {
        DateTime nowdt = rtc.now();
        uint32_t unixs = (uint32_t)nowdt.unixtime();
        setU32Ireg(IR_RTC_UNIX_LO, unixs);
        mb.Ireg(IR_RTC_YEAR, (uint16_t)nowdt.year());
        mb.Ireg(IR_RTC_MONTH, (uint16_t)nowdt.month());
        mb.Ireg(IR_RTC_DAY, (uint16_t)nowdt.day());
        mb.Ireg(IR_RTC_HOUR, (uint16_t)nowdt.hour());
        mb.Ireg(IR_RTC_MIN, (uint16_t)nowdt.minute());
        mb.Ireg(IR_RTC_SEC, (uint16_t)nowdt.second());
    } else {
        setU32Ireg(IR_RTC_UNIX_LO, 0);
        mb.Ireg(IR_RTC_YEAR, 0);
        mb.Ireg(IR_RTC_MONTH, 0);
        mb.Ireg(IR_RTC_DAY, 0);
        mb.Ireg(IR_RTC_HOUR, 0);
        mb.Ireg(IR_RTC_MIN, 0);
        mb.Ireg(IR_RTC_SEC, 0);
    }

    // Snapshot masks + flags
    uint16_t outm = buildOutMaskFromState();
    uint16_t inm = buildInMaskFromState();
    uint16_t dm = buildDirectMaskFromState();
    uint16_t sf = buildSysFlags();

    mb.Ireg(IR_OUTMASK, outm);
    mb.Ireg(IR_INMASK, inm);
    mb.Ireg(IR_DIRECTMASK, dm);
    mb.Ireg(IR_SYSFLAGS, sf);

    // Mirrors in holding regs
    mb.Hreg(HR_OUTMASK_CUR, outm);
    mb.Hreg(HR_INMASK_CUR, inm);

    // Change sequence
    if (outm != g_lastOutMask || inm != g_lastInMask || dm != g_lastDirectMask || sf != g_lastSysFlags) {
        g_changeSeq++;
        mb.Ireg(IR_CHANGE_SEQ, g_changeSeq);
        g_lastOutMask = outm;
        g_lastInMask = inm;
        g_lastDirectMask = dm;
        g_lastSysFlags = sf;
    }
}

void initModbusRtu() {
    // Create map memory
    mb.addCoil(0, false, 20);   // 00001..00020 (includes reserved)
    mb.addIsts(0, false, 24);   // 10001..10024
    mb.addIreg(0, 0, 43);       // 30001..30043
    mb.addHreg(0, 0, 617);      // 40001..40617 (offset 0..616)

    // Set initial coils
    syncCoilsToOutputs();
    mb.Coil(COIL_MASTER_ENABLE, outputsMasterEnable);
    mb.Coil(COIL_NIGHT_MODE, rs485NightMode);


    // Initialize coil snapshots
    g_lastMasterEnable = outputsMasterEnable;
    g_lastNightMode = rs485NightMode;
    // Default status values
    mb.Hreg(HR_MB_APPLY_STATUS, 0);
    mb.Hreg(HR_CMD_STATUS, 0);

    // Start RTU server on RS485 serial (already initialized in initRS485()).
    mb.begin(&rs485);
    mb.slave((uint8_t)rs485DeviceAddress);

    modbusRtuActive = true;
    g_running = true;

    // Initialize identity regs
    refreshIdentityHoldingRegs();
    refreshDiscreteInputs();
    refreshInputRegisters();
}

void taskModbusRtu() {
    if (!g_running) return;

    // First process Modbus frames (this updates coil/register memory on writes)
    mb.task();

    // --- Bidirectional sync between Modbus coils and local output state ---
    // Rule:
    //  - If coil image changed since last snapshot => treat as external Modbus write, apply to outputs.
    //  - Else if local outputs changed => push local state into coil image (so web/UI won't be reverted).
    //
    // Master enable
    bool coilME = mb.Coil(COIL_MASTER_ENABLE);
    if (coilME != g_lastMasterEnable) {
        // External write via Modbus
        g_lastMasterEnable = coilME;
        outputsMasterEnable = coilME;

        if (!outputsMasterEnable) {
            bool any = false;
            for (int i = 0; i < 16; i++) {
                if (outputStates[i]) { outputStates[i] = false; any = true; }
            }
            if (any) writeOutputs();
            // Keep coil image consistent
            syncCoilsToOutputs();
        } else {
            // When enabling, apply current coil image to outputs
            applyOutputsFromCoils();
        }
    } else if (outputsMasterEnable != g_lastMasterEnable) {
        // Local change (e.g., web/UI) - reflect into coil image
        mb.Coil(COIL_MASTER_ENABLE, outputsMasterEnable);
        g_lastMasterEnable = outputsMasterEnable;
    }

    // Night mode
    bool coilNM = mb.Coil(COIL_NIGHT_MODE);
    if (coilNM != g_lastNightMode) {
        g_lastNightMode = coilNM;
        rs485NightMode = coilNM;
    } else if (rs485NightMode != g_lastNightMode) {
        mb.Coil(COIL_NIGHT_MODE, rs485NightMode);
        g_lastNightMode = rs485NightMode;
    }

    // Digital outputs (00001..00016)
    if (!outputsMasterEnable) {
        // Force OFF and keep coil image OFF if disabled
        bool any = false;
        for (int i = 0; i < 16; i++) {
            if (outputStates[i]) { outputStates[i] = false; any = true; }
            if (g_lastDoCoils[i] != false) g_lastDoCoils[i] = false;
            mb.Coil(COIL_DO_START + i, false);
        }
        if (any) writeOutputs();
    } else {
        bool needWrite = false;

        for (int i = 0; i < 16; i++) {
            bool coil = mb.Coil(COIL_DO_START + i);

            if (coil != g_lastDoCoils[i]) {
                // External Modbus write -> apply to outputs
                g_lastDoCoils[i] = coil;
                if (outputStates[i] != coil) {
                    outputStates[i] = coil;
                    needWrite = true;
                }
            } else if (outputStates[i] != g_lastDoCoils[i]) {
                // Local/web change -> push to coil image
                mb.Coil(COIL_DO_START + i, outputStates[i]);
                g_lastDoCoils[i] = outputStates[i];
            }
        }

        if (needWrite) {
            writeOutputs();
        }
    }

    // Apply outmask write helper (also syncs coils->outputs as needed)
    applyOutmaskWriteIfNeeded();

    // Process holding-reg edits
    handleRwStrings();
    handleAnalogCalibrationRw();
    handleMacRw();
    handleApplySaveSerialSettings();
    handleSafeCommands();

    // Keep identity + snapshot updated (also overwrites any attempted writes to RO fields)
    refreshIdentityHoldingRegs();
    refreshDiscreteInputs();
    refreshInputRegisters();
}


bool isModbusRtuRunning() {
    return g_running;
}
