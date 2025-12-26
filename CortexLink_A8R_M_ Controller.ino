#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <WebServer.h>
#include <Preferences.h>

#include "src/Config.h"
#include "src/Debug.h"
#include "src/DigitalInputDriver.h"
#include "src/RelayDriver.h"
#include "src/DACControl.h"
#include "src/AnalogInputs.h"
#include "src/DHT_Sensors.h"
#include "src/RTCManager.h"
#include "src/EthernetDriver.h"
#include "src/ModbusComm.h"

// ======================================================================
// GLOBAL DRIVER INSTANCES
// ======================================================================
RelayDriver          g_relays;
AnalogInputs         g_ai;
DACControl           g_dac;
DHTSensors           g_dht;
RTCManager& g_rtc = rtcManager;
EthernetDriver& g_eth = ethernet;
DigitalInputDriver& g_di = digitalInputs;

// ===== NEW (MODBUS) =====
ModbusComm           g_modbus;

// ======================================================================
// PREFERENCES
// ======================================================================
Preferences g_prefs;


static const String ETH_MAC = "A8:FD:B5:E1:D4:B3";
static const String STA_MAC = "A8:FD:B5:E1:D4:B2";

/* Static AP MAC address */
static const String AP_MAC = "A8:FD:B5:E1:D4:B1";

// Net prefs
static const char* PREF_NS_NET = "netcfg";
static const char* KEY_AP_DONE = "ap_done";
static const char* KEY_ETH_DHCP = "eth_dhcp";
static const char* KEY_ETH_MAC = "eth_mac";
static const char* KEY_ETH_IP = "eth_ip";
static const char* KEY_ETH_MASK = "eth_mask";
static const char* KEY_ETH_GW = "eth_gw";
static const char* KEY_ETH_DNS = "eth_dns";
static const char* KEY_TCP_PORT = "tcp_port";

// Store AP SSID/pass in prefs too
static const char* KEY_AP_SSID = "ap_ssid";
static const char* KEY_AP_PASS = "ap_pass";

// Store last runtime DHCP lease results
static const char* KEY_ETH_IP_CUR = "eth_ip_cur";
static const char* KEY_ETH_MASK_CUR = "eth_m_cur";
static const char* KEY_ETH_GW_CUR = "eth_gw_cur";
static const char* KEY_ETH_DNS_CUR = "eth_dns_cur";

// NEW: analog input voltage range config (software scaling only)
static const char* KEY_AI_VRANGE = "ai_vrng"; // 5 or 10

// ===== NEW (MODBUS prefs) =====
static const char* PREF_NS_MB = "mbcfg";
static const char* KEY_MB_ENABLED = "en";      // bool
static const char* KEY_MB_SLAVEID = "sid";     // u8
static const char* KEY_MB_BAUD = "baud";       // u32
static const char* KEY_MB_PARITY = "par";      // u8 0=N 1=E 2=O
static const char* KEY_MB_STOPBITS = "sb";     // u8 1 or 2
static const char* KEY_MB_DATABITS = "db";     // u8 7 or 8

// Serial settings prefs namespace/keys (NEW)
static const char* PREF_NS_SER = "sercfg";
static const char* KEY_SER_BAUD = "baud";
static const char* KEY_SER_PAR = "par";   // 0=N 1=O 2=E
static const char* KEY_SER_SB = "sb";    // 1/2
static const char* KEY_SER_DB = "db";    // 7/8

// Scheduler prefs (fixed maximum number of schedules)
static const uint8_t  SCHED_MAX = 8;
static const char* PREF_NS_SCHED_META = "schedm";
static const char* KEY_SCHED_COUNT = "count";

// Keys within each schedule namespace "sch0".."sch7"
static const char* KEY_S_EN = "en";
static const char* KEY_S_MODE = "mode";   // expanded
static const char* KEY_S_DI = "di";       // 1..8
static const char* KEY_S_EDGE = "edge";   // 0=R 1=F 2=B
static const char* KEY_S_ST = "st";       // start minutes
static const char* KEY_S_ENM = "enM";     // end minutes
static const char* KEY_S_RECUR = "recur"; // kept for compat
static const char* KEY_S_DAYS = "days";   // bitmask Mon..Sun (bit0=Mon..bit6=Sun)

static const char* KEY_S_RMASK = "rmask"; // 0..63
static const char* KEY_S_RACT = "ract";   // 0=ON 1=OFF 2=TOGGLE
static const char* KEY_S_D1MV = "d1mv";
static const char* KEY_S_D2MV = "d2mv";
static const char* KEY_S_D1R = "d1r";
static const char* KEY_S_D2R = "d2r";
static const char* KEY_S_BUZEN = "buzen";
static const char* KEY_S_BUZF = "buzf";
static const char* KEY_S_BUZON = "buzon";
static const char* KEY_S_BUZOFF = "buzoff";
static const char* KEY_S_BUZREP = "buzrep";

// NEW: Analog Trigger keys
static const char* KEY_A_EN = "a_en";
static const char* KEY_A_TYPE = "a_ty";      // 0=VOLT 1=CURR
static const char* KEY_A_CH = "a_ch";        // 1..2
static const char* KEY_A_OP = "a_op";        // 0=ABOVE 1=BELOW 2=EQUAL 3=IN_RANGE
static const char* KEY_A_V1 = "a_v1";        // mV or mA
static const char* KEY_A_V2 = "a_v2";        // mV or mA (for range)
static const char* KEY_A_HYS = "a_hys";      // hysteresis (mV or mA)
static const char* KEY_A_DBMS = "a_db";      // debounce ms
static const char* KEY_A_TOL = "a_tol";      // tolerance for EQUAL (mV or mA)

// ======================================================================
// WIFI AP / CONFIG PORTAL
// ======================================================================
WebServer g_http(80);
String g_apSsid = "A8RM-SETUP";
String g_apPass = "cortexlink";
bool   g_runConfigPortal = false;

// ======================================================================
// ETHERNET TCP SERVER
// ======================================================================
EthernetServer* g_tcpServer = nullptr;
EthernetClient  g_tcpClient;
uint16_t        g_tcpPort = 5000;
unsigned long   g_lastClientRx = 0;
const uint32_t  CLIENT_TIMEOUT_MS = 30000;

// ======================================================================
// SERIAL CONSOLE
// ======================================================================
String  g_serialCmd;
bool    g_serialCmdReady = false;

// ======================================================================
// SCHEDULER
// ======================================================================

// Existing values preserved for backward compatibility:
// 0=DI 1=RTC 2=COMBINED(DI+RTC)
enum SchedMode : uint8_t {
    S_MODE_DI = 0,
    S_MODE_RTC = 1,
    S_MODE_COMBINED = 2,

    // NEW
    S_MODE_ANALOG = 3,
    S_MODE_ANALOG_DI = 4,        // OR logic
    S_MODE_ANALOG_RTC = 5,       // OR logic
    S_MODE_ANALOG_DI_RTC = 6     // OR logic
};

// Existing values preserved for backward compatibility:
// 0=RISING 1=FALLING 2=BOTH
// NEW:
// 3=HIGH (level) 4=LOW (level)
enum EdgeMode : uint8_t {
    EDGE_RISING = 0,
    EDGE_FALLING = 1,
    EDGE_BOTH = 2,
    EDGE_HIGH = 3,
    EDGE_LOW = 4
};
enum RelayAct : uint8_t { RELACT_ON = 0, RELACT_OFF = 1, RELACT_TOGGLE = 2 };

enum AnalogType : uint8_t { A_TYPE_VOLT = 0, A_TYPE_CURR = 1 };
enum AnalogOp : uint8_t { A_OP_ABOVE = 0, A_OP_BELOW = 1, A_OP_EQUAL = 2, A_OP_IN_RANGE = 3 };

struct AnalogTriggerConfig {
    bool enabled = false;
    AnalogType type = A_TYPE_VOLT;   // VOLT or CURR
    uint8_t ch = 1;                  // 1..2
    AnalogOp op = A_OP_ABOVE;

    int32_t v1 = 0;                  // mV or mA
    int32_t v2 = 0;                  // mV or mA
    int32_t hysteresis = 50;         // mV or mA
    uint16_t debounceMs = 200;       // stable-time requirement
    int32_t tol = 50;                // tolerance for EQUAL (mV or mA)

    // runtime
    bool condition = false;          // last stable condition
    bool pending = false;            // current raw condition (pre-debounce)
    uint32_t pendingSinceMs = 0;     // when pending started
    uint32_t lastFireMs = 0;         // anti-spam (optional)
};

struct ScheduleConfig {
    bool enabled = false;
    SchedMode mode = S_MODE_DI;

    uint8_t triggerDI = 1;     // 1..8
    EdgeMode edge = EDGE_RISING;

    uint16_t startMin = 8 * 60;
    uint16_t endMin = 17 * 60;

    uint8_t daysMask = 0x7F;   // Mon..Sun
    bool recurring = true;     // kept

    uint8_t relayMask = 0;     // bits 0..5
    RelayAct relayAct = RELACT_TOGGLE;

    int dac1mV = 0;
    int dac2mV = 0;
    uint8_t dac1Range = 5;
    uint8_t dac2Range = 5;

    bool     buzEnable = false;
    uint16_t buzFreq = 2000;
    uint16_t buzOnMs = 200;
    uint16_t buzOffMs = 200;
    uint8_t  buzRepeats = 5;

    // NEW
    AnalogTriggerConfig a;

    // runtime
    bool firedForWindow = false;
    uint32_t lastDiEdgeMs = 0;
};

static ScheduleConfig g_scheds[SCHED_MAX];
static uint8_t        g_schedCount = 0;
static uint8_t g_lastDiBits = 0;

// Buzzer runtime
struct BuzzPatternRuntime {
    bool active = false;
    bool phaseOn = false;
    uint16_t freq = 2000;
    uint16_t onMs = 200;
    uint16_t offMs = 200;
    uint8_t repeats = 5;
    uint8_t step = 0;
    uint32_t phaseStartMs = 0;
} g_buzz;

// ======================================================================
// Device Info Preferences
// ======================================================================
static const char* PREF_NS_HWINFO = "hwinfo";
static const char* KEY_BOARD_NAME = "board_name";
static const char* KEY_BOARD_SERIAL = "board_sn";
static const char* KEY_CREATED = "created";
static const char* KEY_MANUFACTURER = "mfg";
static const char* KEY_FW_VER = "fw";
static const char* KEY_HW_VER = "hw";
static const char* KEY_YEAR = "year";

static const char* FW_VERSION_STR = "1.0.0";
static const char* HW_VERSION_STR = "A8R-M-REV1";
static const char* MANUFACTURER_STR = "Microcode Engineering";

// ===== NEW: MAC info block =====
static const uint16_t MB_REG_MACINFO_START = 290;     // 290..316 (27 regs)
static const uint16_t MB_MACINFO_REGS = 27;

// Add new holding-register storage
static uint16_t mb_macInfoRegs[MB_MACINFO_REGS] = { 0 };

// NEW: get Ethernet MAC from prefs (KEY_ETH_MAC is already used elsewhere)
static String getEthMacFromPrefs() {

    g_prefs.begin(PREF_NS_NET, true);
    String mac = g_prefs.getString(KEY_ETH_MAC, ETH_MAC);
    g_prefs.end();
    return mac;
}

bool parseMacString(const String& macStr, uint8_t* mac)
{
    if (macStr.length() != 17) return false;

    for (int i = 0; i < 6; i++)
    {
        mac[i] = strtoul(macStr.substring(i * 3, i * 3 + 2).c_str(), NULL, 16);
    }
    return true;
}

// NEW: refresh MAC info registers (17 chars -> 9 regs)
static void refreshMacInfoRegs() {
    // Ethernet MAC (from stored prefs, used by EthernetDriver/W5500 init)
    String ethMac = getEthMacFromPrefs();

    // WiFi MACs (available even if WiFi not connected)
  //String staMac = WiFi.macAddress();          // "AA:BB:CC:DD:EE:FF"
    //String apMac = WiFi.softAPmacAddress();    // valid after softAP, but still returns something

    g_prefs.begin("wifi_cfg", true);
    String apMac = g_prefs.getString("ap_mac", "00:00:00:00:00:00");
    g_prefs.end();

    g_prefs.begin("wifi_cfg", true);
    String staMac = g_prefs.getString("sta_mac", "00:00:00:00:00:00");
    g_prefs.end();

    // Enforce max length 17 (we pack into 9 regs = 18 chars space)
    if (ethMac.length() > 17) ethMac = ethMac.substring(0, 17);
    if (staMac.length() > 17) staMac = staMac.substring(0, 17);
    if (apMac.length() > 17)  apMac = apMac.substring(0, 17);

    // Layout:
    // 290..298 => ETH_MAC (9 regs)
    // 299..307 => WIFI_STA_MAC (9 regs)
    // 308..316 => WIFI_AP_MAC (9 regs)
    writePackedStringToRegs(&mb_macInfoRegs[0], 9, ethMac);
    writePackedStringToRegs(&mb_macInfoRegs[9], 9, staMac);
    writePackedStringToRegs(&mb_macInfoRegs[18], 9, apMac);
}

// NEW: Modbus get callback for the MAC block
static uint16_t mbMacInfoGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_MACINFO_START;
    return (idx < MB_MACINFO_REGS) ? mb_macInfoRegs[idx] : 0;
}

// ======================================================================
// ===== MODBUS REGISTER STORAGE (mirrors MODBUS_Test.ino style) =====
// ======================================================================
static uint16_t mb_inputRegs[16] = { 0 };   // digital inputs
static uint16_t mb_relayRegs[8] = { 0 };    // relay outputs logical 0/1
static uint16_t mb_analogRegs[8] = { 0 };   // raw/scaled pairs for 2V + 2I => 8 regs
static uint16_t mb_tempRegs[8] = { 0 };
static uint16_t mb_humRegs[8] = { 0 };
static uint16_t mb_ds18Regs[8] = { 0 };
static uint16_t mb_dacRegs[4] = { 0 };      // 0 raw0,1 raw1,2 volts*100 ch0,3 volts*100 ch1
static uint16_t mb_rtcRegs[8] = { 0 };

// ===== NEW: extra holding-register storage =====
static uint16_t mb_sysInfoRegs[58] = { 0 }; // 100..157
static uint16_t mb_netInfoRegs[50] = { 0 }; // 170..219
static uint16_t mb_mbSetRegs[5] = { 0 }; // 230..234
static uint16_t mb_serSetRegs[4] = { 0 }; // 240..243
static uint16_t mb_buzRegs[6] = { 0 }; // 250..255

// MODBUS settings (runtime)
struct ModbusSettings {
    bool enabled = true;
    uint8_t slaveId = 1;
    uint32_t baud = 38400;
    uint8_t dataBits = 8;     // 7 or 8
    uint8_t parity = 0;       // 0=N 1=E 2=O
    uint8_t stopBits = 1;     // 1 or 2
} g_mbCfg;

struct SerialSettings {
    uint32_t baud = 115200;
    uint8_t dataBits = 8;
    uint8_t parity = 0;
    uint8_t stopBits = 1;
} g_serCfg;


// Shared baud table indices for BOTH Modbus RTU and Serial settings registers.
// We store *index* in Modbus HR registers (uint16), not raw baud.
static const uint32_t BAUD_TABLE[] = {
  1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};
static const uint8_t BAUD_TABLE_COUNT = sizeof(BAUD_TABLE) / sizeof(BAUD_TABLE[0]);

static uint8_t baudToIndex(uint32_t baud) {
    for (uint8_t i = 0; i < BAUD_TABLE_COUNT; i++) {
        if (BAUD_TABLE[i] == baud) return i;
    }
    // default to 9600 for Modbus / 115200 for Serial depends on usage, but we keep a safe default.
    // Here: 9600 index if present, else 0.
    for (uint8_t i = 0; i < BAUD_TABLE_COUNT; i++) {
        if (BAUD_TABLE[i] == 9600) return i;
    }
    return 0;
}

static uint32_t indexToBaud(uint16_t idx, uint32_t fallback) {
    if (idx < BAUD_TABLE_COUNT) return BAUD_TABLE[(uint8_t)idx];
    return fallback;
}


// ======================================================================
// Helper: format chip unique MAC
// ======================================================================
static String espMacToString() {
    uint64_t mac = ESP.getEfuseMac();
    uint8_t b[6];
    b[0] = (mac >> 40) & 0xFF;
    b[1] = (mac >> 32) & 0xFF;
    b[2] = (mac >> 24) & 0xFF;
    b[3] = (mac >> 16) & 0xFF;
    b[4] = (mac >> 8) & 0xFF;
    b[5] = (mac >> 0) & 0xFF;

    char s[18];
    snprintf(s, sizeof(s), "%02X:%02X:%02X:%02X:%02X:%02X",
        b[0], b[1], b[2], b[3], b[4], b[5]);
    return String(s);
}

// ======================================================================
// FORWARD DECLARATIONS
// ======================================================================
void startConfigPortal();
void stopConfigPortal();
void startConfigStation_WiFi();
void loadNetworkConfig();
bool startEthernet();
void startTcpServer();
void handleTcpClient();
void handleTcpCommand(const String& line);
void sendSnapshot(EthernetClient& c, bool oneLineJson);

void handleHttpRoot();
void handleHttpGetConfig();
void handleHttpSetConfig();
void handleHttpNotFound();

void processSerial();
void execSerialCommand(const String& cmd);
void printHelp();
void printStatus();

static String ipToString(IPAddress ip);
static bool parseHHMM(const String& s, uint16_t& outMin);
static String fmtHHMM(uint16_t mins);
static uint16_t rtcMinutesNow();
static uint8_t rtcDaysMaskNowMonBit0();
static bool isWithinWindow(uint16_t nowMin, uint16_t startMin, uint16_t endMin);

static void schedLoadAll();
static void schedSaveMeta();
static void schedSaveOne(uint8_t idx);
static void schedDelete(uint8_t idx);
static void schedResetAll();
static void schedUpdate();
static void schedApplyOutputs(const ScheduleConfig& s);

static void buzzStart(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t repeats);
static void buzzStop();
static void buzzUpdate();

static String getTokValue(const String& args, const String& key);
static String schedNs(uint8_t idx);

// NETCFG helpers
static void netcfgReplyGet();
static void netcfgApplySet(const String& args);
static bool netcfgIsSafeToken(const String& v);
static void netcfgScheduleReboot(uint32_t delayMs);
static void netcfgSerialDump();
static void netcfgPersistRuntimeLease();

// AI config helpers
static void loadAnalogConfig();
static void saveAnalogConfig(uint8_t vRange);

// Board info
static void hwInfoEnsureDefaults();
static void hwInfoPrintBoardInfoSerial();
static void hwInfoPrintBoardInfoTcp();

// ===== NEW (MODBUS helpers) =====
static void loadModbusConfig();
static void saveModbusConfig();
static void initModbus();               // keep same name as MODBUS_Test.ino
static void updateModbusData();         // refresh mb_* arrays from drivers

// MODBUS callbacks (pattern from MODBUS_Test.ino)
static uint16_t mbInputsCallback(TRegister* reg, uint16_t val);
static uint16_t mbDiscreteInputsCallback(TRegister* reg, uint16_t val);
static uint16_t mbRelaysGet(TRegister* reg, uint16_t val);
static uint16_t mbRelaysSet(TRegister* reg, uint16_t val);
static uint16_t mbRelaysCoilGet(TRegister* reg, uint16_t val);
static uint16_t mbRelaysCoilSet(TRegister* reg, uint16_t val);
static uint16_t mbAnalogCallback(TRegister* reg, uint16_t val);
static uint16_t mbDacGet(TRegister* reg, uint16_t val);
static uint16_t mbDacSet(TRegister* reg, uint16_t val);
static uint16_t mbRtcGet(TRegister* reg, uint16_t val);
static uint16_t mbRtcSet(TRegister* reg, uint16_t val);

// ======================================================================
// Utility
// ======================================================================



String ipToString(IPAddress ip) {
    char b[24];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return String(b);
}

static String getTokValue(const String& args, const String& key) {
    String k = key + "=";
    int p = args.indexOf(k);
    if (p < 0) return "";
    int s = p + k.length();
    int e = args.indexOf(' ', s);
    if (e < 0) e = args.length();
    return args.substring(s, e);
}

static String schedNs(uint8_t idx) {
    return String("sch") + String(idx);
}

static void tcpSend(const String& s) {
    if (g_tcpClient && g_tcpClient.connected()) g_tcpClient.println(s);
}

static bool netcfgIsSafeToken(const String& v) {
    if (v.indexOf(' ') >= 0) return false;
    if (v.indexOf('\r') >= 0) return false;
    if (v.indexOf('\n') >= 0) return false;
    return true;
}

// Reboot scheduler (non-blocking)
static bool g_rebootPending = false;
static uint32_t g_rebootAtMs = 0;
static void netcfgScheduleReboot(uint32_t delayMs) {
    g_rebootPending = true;
    g_rebootAtMs = millis() + delayMs;
}

static void netcfgPersistRuntimeLease() {
    if (!g_eth.isReady()) return;

    IPAddress ip = g_eth.getIP();
    IPAddress mask = g_eth.getSubnetMask();
    IPAddress gw = g_eth.getGateway();
    IPAddress dns = g_eth.getDNSServer();

    g_prefs.begin(PREF_NS_NET, false);
    g_prefs.putString(KEY_ETH_IP_CUR, ipToString(ip));
    g_prefs.putString(KEY_ETH_MASK_CUR, ipToString(mask));
    g_prefs.putString(KEY_ETH_GW_CUR, ipToString(gw));
    g_prefs.putString(KEY_ETH_DNS_CUR, ipToString(dns));
    g_prefs.end();
}

// ======================================================================
// Analog config
// ======================================================================
static void loadAnalogConfig() {
    g_prefs.begin(PREF_NS_NET, true);
    uint8_t vr = (uint8_t)g_prefs.getUChar(KEY_AI_VRANGE, 5);
    g_prefs.end();
    if (vr != 10) vr = 5;
    g_ai.setVoltageRange(vr == 10 ? AnalogInputs::V_RANGE_10V : AnalogInputs::V_RANGE_5V);
}

static void saveAnalogConfig(uint8_t vRange) {
    if (vRange != 10) vRange = 5;
    g_ai.setVoltageRange(vRange == 10 ? AnalogInputs::V_RANGE_10V : AnalogInputs::V_RANGE_5V);
    g_prefs.begin(PREF_NS_NET, false);
    g_prefs.putUChar(KEY_AI_VRANGE, vRange);
    g_prefs.end();
}

// ======================================================================
// ===== MODBUS CONFIG (Preferences) =====
// ======================================================================
static void loadModbusConfig() {
    g_prefs.begin(PREF_NS_MB, true);
    g_mbCfg.enabled = g_prefs.getBool(KEY_MB_ENABLED, true);
    g_mbCfg.slaveId = g_prefs.getUChar(KEY_MB_SLAVEID, 1);
    g_mbCfg.baud = g_prefs.getULong(KEY_MB_BAUD, 38400);

    g_mbCfg.parity = g_prefs.getUChar(KEY_MB_PARITY, 0);
    g_mbCfg.stopBits = g_prefs.getUChar(KEY_MB_STOPBITS, 1);
    g_mbCfg.dataBits = g_prefs.getUChar(KEY_MB_DATABITS, 8);
    g_prefs.end();

    if (g_mbCfg.slaveId < 1) g_mbCfg.slaveId = 1;
    if (g_mbCfg.slaveId > 247) g_mbCfg.slaveId = 1;
    if (g_mbCfg.baud < 1200) g_mbCfg.baud = 9600;
    if (g_mbCfg.baud > 2000000) g_mbCfg.baud = 9600;
    if (g_mbCfg.parity > 2) g_mbCfg.parity = 0;
    if (g_mbCfg.stopBits != 2) g_mbCfg.stopBits = 1;
    if (g_mbCfg.dataBits != 7) g_mbCfg.dataBits = 8; // default 8 unless explicitly 7
}

static void saveModbusConfig() {
    g_prefs.begin(PREF_NS_MB, false);
    g_prefs.putBool(KEY_MB_ENABLED, g_mbCfg.enabled);
    g_prefs.putUChar(KEY_MB_SLAVEID, g_mbCfg.slaveId);
    g_prefs.putULong(KEY_MB_BAUD, g_mbCfg.baud);
    g_prefs.putUChar(KEY_MB_PARITY, g_mbCfg.parity);
    g_prefs.putUChar(KEY_MB_STOPBITS, g_mbCfg.stopBits);
    g_prefs.putUChar(KEY_MB_DATABITS, g_mbCfg.dataBits);
    g_prefs.end();
}

// ===== Helpers: pack/unpack 2 ASCII bytes per register =====
static inline uint16_t pack2(const char a, const char b) {
    return ((uint16_t)(uint8_t)a << 8) | (uint16_t)(uint8_t)b;
}
static inline void writePackedStringToRegs(uint16_t* base, uint16_t regCount, const String& s) {
    // regCount registers => regCount*2 chars max
    const uint16_t maxChars = regCount * 2;
    for (uint16_t i = 0; i < regCount; i++) base[i] = 0;

    for (uint16_t i = 0; i < maxChars; i += 2) {
        char c1 = (i < s.length()) ? s[i] : '\0';
        char c2 = (i + 1 < s.length()) ? s[i + 1] : '\0';
        base[i / 2] = pack2(c1, c2);
    }
}
static inline String readPackedStringFromRegs(const uint16_t* base, uint16_t regCount) {
    String out;
    out.reserve(regCount * 2);
    for (uint16_t i = 0; i < regCount; i++) {
        char c1 = (char)((base[i] >> 8) & 0xFF);
        char c2 = (char)(base[i] & 0xFF);
        if (c1) out += c1;
        if (c2) out += c2;
    }
    out.trim();
    return out;
}

static void refreshSysInfoRegs() {
    // Read from hwinfo prefs + runtime MAC
    g_prefs.begin(PREF_NS_HWINFO, true);
    String board = g_prefs.getString(KEY_BOARD_NAME, "KC-Link A8R-M");
    String sn = g_prefs.getString(KEY_BOARD_SERIAL, "");
    String mfg = g_prefs.getString(KEY_MANUFACTURER, MANUFACTURER_STR);
    String fw = g_prefs.getString(KEY_FW_VER, FW_VERSION_STR);
    String hw = g_prefs.getString(KEY_HW_VER, HW_VERSION_STR);
    String yearS = g_prefs.getString(KEY_YEAR, "2025");
    g_prefs.end();

    // Enforce max lengths per map (prevents confusing truncations)
    if (board.length() > 32) board = board.substring(0, 32);
    if (sn.length() > 32) sn = sn.substring(0, 32);
    if (mfg.length() > 16) mfg = mfg.substring(0, 16);
    if (fw.length() > 8) fw = fw.substring(0, 8);
    if (hw.length() > 8) hw = hw.substring(0, 8);

    String mac = espMacToString(); // "AA:BB:CC:DD:EE:FF" (17 chars), but your map says 16 chars.
    // Your map is 16 chars. "AA:BB:CC:DD:EE:FF" is 17 including all colons.
    // So we must truncate to 16 (it will lose the last char). Better alternative:
    // store without last colon segment? But requirement says it fits; it doesn't in 16.
    // Best within your constraints: store "AA:BB:CC:DD:EE:" (16) OR store without separators (12).
    // We'll store 16-char truncated so VB displays consistently:
    if (mac.length() > 17) mac = mac.substring(0, 17);

    // Layout offsets inside mb_sysInfoRegs (reg100 base):
    writePackedStringToRegs(&mb_sysInfoRegs[0], 16, board);  // 100..115
    writePackedStringToRegs(&mb_sysInfoRegs[16], 16, sn);    // 116..131
    writePackedStringToRegs(&mb_sysInfoRegs[32], 8, mfg);    // 132..139
    writePackedStringToRegs(&mb_sysInfoRegs[40], 9, mac);    // 140..148
    writePackedStringToRegs(&mb_sysInfoRegs[49], 4, fw);     // 149..152
    writePackedStringToRegs(&mb_sysInfoRegs[53], 4, hw);     // 153..156

    uint16_t year = (uint16_t)yearS.toInt();
    if (year < 1970 || year > 2100) year = 2025;
    mb_sysInfoRegs[57] = year; // 157
}


static void refreshNetInfoRegs() {
    // Read prefs + runtime (prefer runtime)
    g_prefs.begin(PREF_NS_NET, true);
    bool dhcp = g_prefs.getBool(KEY_ETH_DHCP, true);
    String ipPref = g_prefs.getString(KEY_ETH_IP, "");
    String maskPref = g_prefs.getString(KEY_ETH_MASK, "255.255.255.0");
    String gwPref = g_prefs.getString(KEY_ETH_GW, "");
    String dnsPref = g_prefs.getString(KEY_ETH_DNS, "8.8.8.8");
    uint16_t port = g_prefs.getUShort(KEY_TCP_PORT, 5000);
    String apS = g_prefs.getString(KEY_AP_SSID, g_apSsid);
    String apP = g_prefs.getString(KEY_AP_PASS, g_apPass);
    g_prefs.end();

    String ipNow = g_eth.isReady() ? ipToString(g_eth.getIP()) : ipPref;
    String maskNow = g_eth.isReady() ? ipToString(g_eth.getSubnetMask()) : maskPref;
    String gwNow = g_eth.isReady() ? ipToString(g_eth.getGateway()) : gwPref;
    String dnsNow = g_eth.isReady() ? ipToString(g_eth.getDNSServer()) : dnsPref;

    // 170..177 IP (8 regs)
    // 178..185 MASK (8)
    // 186..193 GW (8)
    // 194..201 DNS (8)
    // 202 DHCP
    // 203 TCPPORT
    // 204..211 AP_SSID (8)
    // 212..219 AP_PASS (8)

    writePackedStringToRegs(&mb_netInfoRegs[0], 8, ipNow);
    writePackedStringToRegs(&mb_netInfoRegs[8], 8, maskNow);
    writePackedStringToRegs(&mb_netInfoRegs[16], 8, gwNow);
    writePackedStringToRegs(&mb_netInfoRegs[24], 8, dnsNow);

    mb_netInfoRegs[32] = dhcp ? 1 : 0;     // reg 202
    mb_netInfoRegs[33] = port;             // reg 203

    writePackedStringToRegs(&mb_netInfoRegs[34], 8, apS);
    writePackedStringToRegs(&mb_netInfoRegs[42], 8, apP);
}

static void refreshMbSettingsRegs() {
    // Store indexes, not raw baud (uint16-safe)
    mb_mbSetRegs[0] = g_mbCfg.slaveId;                  // 230
    mb_mbSetRegs[1] = baudToIndex(g_mbCfg.baud);        // 231 (INDEX)
    mb_mbSetRegs[2] = g_mbCfg.dataBits;                 // 232
    mb_mbSetRegs[3] = g_mbCfg.parity;                   // 233 (0=N 1=O 2=E)
    mb_mbSetRegs[4] = g_mbCfg.stopBits;                 // 234
}

static void refreshSerialSettingsRegs() {
    // Store indexes, not raw baud (uint16-safe)
    mb_serSetRegs[0] = baudToIndex(g_serCfg.baud);  // 240 (INDEX)
    mb_serSetRegs[1] = g_serCfg.dataBits;           // 241
    mb_serSetRegs[2] = g_serCfg.parity;             // 242
    mb_serSetRegs[3] = g_serCfg.stopBits;           // 243
}

static uint32_t serialConfigUsb(uint8_t db, uint8_t par, uint8_t sb) {
    if (db != 7 && db != 8) db = 8;
    if (par > 2) par = 0;
    if (sb != 2) sb = 1;

    if (db == 8) {
        if (sb == 1) return (par == 0) ? SERIAL_8N1 : (par == 1) ? SERIAL_8O1 : SERIAL_8E1;
        else         return (par == 0) ? SERIAL_8N2 : (par == 1) ? SERIAL_8O2 : SERIAL_8E2;
    }
    else {
        if (sb == 1) return (par == 0) ? SERIAL_7N1 : (par == 1) ? SERIAL_7O1 : SERIAL_7E1;
        else         return (par == 0) ? SERIAL_7N2 : (par == 1) ? SERIAL_7O2 : SERIAL_7E2;
    }
}

// ===== FIX: applyUsbSerialSettingsFromRegs() must treat baud as INDEX =====
static void applyUsbSerialSettingsFromRegs() {
    // mb_serSetRegs[0] is BAUD INDEX, not raw baud
    uint16_t baudIdx = mb_serSetRegs[0];
    uint32_t baud = indexToBaud(baudIdx, 115200);

    uint8_t db = (uint8_t)mb_serSetRegs[1];
    uint8_t par = (uint8_t)mb_serSetRegs[2];
    uint8_t sb = (uint8_t)mb_serSetRegs[3];

    uint32_t cfg = serialConfigUsb(db, par, sb);

    Serial.flush();
    Serial.end();
    delay(50);
    Serial.begin(baud, cfg);
    delay(50);

    INFO_LOG("USB Serial updated via Modbus: baudIdx=%u baud=%lu db=%u par=%u sb=%u",
        (unsigned)baudIdx, (unsigned long)baud, db, par, sb);
}

static void refreshBuzzerRegsDefaults() {
    if (mb_buzRegs[0] == 0) {
        mb_buzRegs[0] = 2000; // freq
        mb_buzRegs[1] = 200;  // ms
        mb_buzRegs[2] = 200;  // pattern on
        mb_buzRegs[3] = 200;  // pattern off
        mb_buzRegs[4] = 5;    // repeats
        mb_buzRegs[5] = 0;    // cmd
    }
}


static uint16_t mbNetInfoSet(TRegister* reg, uint16_t val) {
    // Allow writing DHCP/TCPPORT/APSSID/APPASS + optionally static IP strings
    // For safety, we will write only known scalar regs + AP fields; others can be added.
    uint16_t addr = reg->address.address;
    uint16_t idx = addr - MB_REG_NETINFO_START;

    if (idx >= 50) return reg->value;

    // Update mirror
    mb_netInfoRegs[idx] = val;
    reg->value = val;

    // If writing DHCP (202) or TCPPORT (203), persist it.
    if (addr == 202 || addr == 203) {
        bool dhcp = mb_netInfoRegs[32] != 0;
        uint16_t tcpPort = mb_netInfoRegs[33];
        if (tcpPort < 1) tcpPort = 1;
        if (tcpPort > 65535) tcpPort = 65535;

        g_prefs.begin(PREF_NS_NET, false);
        g_prefs.putBool(KEY_ETH_DHCP, dhcp);
        g_prefs.putUShort(KEY_TCP_PORT, tcpPort);
        g_prefs.end();

        // Apply runtime TCP port
        g_tcpPort = tcpPort;

        // recommend reboot for full network restart
        tcpSend("NETCFG OK"); // harmless if no TCP client
    }

    // AP SSID/PASS blocks: 204..219 => idx 34..49
    if (addr >= 204 && addr <= 219) {
        String apS = readPackedStringFromRegs(&mb_netInfoRegs[34], 8);
        String apP = readPackedStringFromRegs(&mb_netInfoRegs[42], 8);

        g_prefs.begin(PREF_NS_NET, false);
        g_prefs.putString(KEY_AP_SSID, apS);
        g_prefs.putString(KEY_AP_PASS, apP);
        g_prefs.end();

        g_apSsid = apS;
        g_apPass = apP;
    }

    return reg->value;
}



// Helpers: pack/unpack ASCII (2 chars per uint16 register, hi-byte then lo-byte)
static void packAsciiToRegs(uint16_t* outRegs, uint16_t regCount, const String& s) {
    memset(outRegs, 0, regCount * sizeof(uint16_t));
    const size_t maxChars = (size_t)regCount * 2;
    String t = s;
    if (t.length() > (int)maxChars) t = t.substring(0, (int)maxChars);

    for (size_t i = 0; i < (size_t)t.length(); i++) {
        uint8_t ch = (uint8_t)t[i];
        size_t r = i / 2;
        bool hi = ((i % 2) == 0);
        if (r >= regCount) break;
        if (hi) outRegs[r] |= ((uint16_t)ch << 8);
        else    outRegs[r] |= ((uint16_t)ch);
    }
}

static String readAsciiFromRegs(const uint16_t* regs, uint16_t regCount) {
    String s;
    s.reserve(regCount * 2);
    for (uint16_t i = 0; i < regCount; i++) {
        uint8_t hi = (uint8_t)((regs[i] >> 8) & 0xFF);
        uint8_t lo = (uint8_t)(regs[i] & 0xFF);
        if (hi) s += (char)hi;
        if (lo) s += (char)lo;
    }
    s.trim();
    return s;
}

static void loadSerialConfig() {
    g_prefs.begin(PREF_NS_SER, true);
    g_serCfg.baud = g_prefs.getULong(KEY_SER_BAUD, 115200);
    g_serCfg.parity = g_prefs.getUChar(KEY_SER_PAR, 0);
    g_serCfg.stopBits = g_prefs.getUChar(KEY_SER_SB, 1);
    g_serCfg.dataBits = g_prefs.getUChar(KEY_SER_DB, 8);
    g_prefs.end();

    if (g_serCfg.baud < 1200) g_serCfg.baud = 115200;
    if (g_serCfg.baud > 2000000) g_serCfg.baud = 115200;
    if (g_serCfg.parity > 2) g_serCfg.parity = 0;
    if (g_serCfg.stopBits != 2) g_serCfg.stopBits = 1;
    if (g_serCfg.dataBits != 7) g_serCfg.dataBits = 8;
}

static void saveSerialConfig() {
    g_prefs.begin(PREF_NS_SER, false);
    g_prefs.putULong(KEY_SER_BAUD, g_serCfg.baud);
    g_prefs.putUChar(KEY_SER_PAR, g_serCfg.parity);
    g_prefs.putUChar(KEY_SER_SB, g_serCfg.stopBits);
    g_prefs.putUChar(KEY_SER_DB, g_serCfg.dataBits);
    g_prefs.end();
}

// Refresh the new Modbus mirror arrays
static void updateModbusSystemInfoRegs() {
    // Board info comes from PREF_NS_HWINFO + efuse MAC
    g_prefs.begin(PREF_NS_HWINFO, true);
    String board = g_prefs.getString(KEY_BOARD_NAME, "KC_Link A8R-M");
    String sn = g_prefs.getString(KEY_BOARD_SERIAL, "");
    String mfg = g_prefs.getString(KEY_MANUFACTURER, MANUFACTURER_STR);
    String fw = g_prefs.getString(KEY_FW_VER, FW_VERSION_STR);
    String hw = g_prefs.getString(KEY_HW_VER, HW_VERSION_STR);
    String year = g_prefs.getString(KEY_YEAR, "2025");
    g_prefs.end();

    String mac = espMacToString();

    // 100..115 board name (32 chars = 16 regs)
    packAsciiToRegs(&mb_sysInfoRegs[0], 16, board);
    // 116..131 serial (32 chars = 16 regs)
    packAsciiToRegs(&mb_sysInfoRegs[16], 16, sn);
    // 132..139 manufacturer (16 chars = 8 regs)
    packAsciiToRegs(&mb_sysInfoRegs[32], 8, mfg);
    // 140..148 MAC (17 chars = 9 regs)
    packAsciiToRegs(&mb_sysInfoRegs[40], 9, mac);
    // 149..152 FW (8 chars = 4 regs)
    packAsciiToRegs(&mb_sysInfoRegs[49], 4, fw);
    // 153..156 HW (8 chars = 4 regs)
    packAsciiToRegs(&mb_sysInfoRegs[53], 4, hw);
    // 157 year (uint16)
    mb_sysInfoRegs[57] = (uint16_t)year.toInt();
}

static void updateModbusNetworkInfoRegs() {
    g_prefs.begin(PREF_NS_NET, true);
    bool dhcp = g_prefs.getBool(KEY_ETH_DHCP, true);
    String ipPref = g_prefs.getString(KEY_ETH_IP, "");
    String maskPref = g_prefs.getString(KEY_ETH_MASK, "255.255.255.0");
    String gwPref = g_prefs.getString(KEY_ETH_GW, "");
    String dnsPref = g_prefs.getString(KEY_ETH_DNS, "8.8.8.8");
    uint16_t port = g_prefs.getUShort(KEY_TCP_PORT, 5000);
    String apS = g_prefs.getString(KEY_AP_SSID, g_apSsid);
    String apP = g_prefs.getString(KEY_AP_PASS, g_apPass);
    g_prefs.end();

    // Use CURRENT runtime values when ready (preferred)
    String ipNow = g_eth.isReady() ? ipToString(g_eth.getIP()) : ipPref;
    String maskNow = g_eth.isReady() ? ipToString(g_eth.getSubnetMask()) : maskPref;
    String gwNow = g_eth.isReady() ? ipToString(g_eth.getGateway()) : gwPref;
    String dnsNow = g_eth.isReady() ? ipToString(g_eth.getDNSServer()) : dnsPref;

    // 170..177 IP (16 chars = 8 regs)
    packAsciiToRegs(&mb_netInfoRegs[0], 8, ipNow);
    // 178..185 MASK
    packAsciiToRegs(&mb_netInfoRegs[8], 8, maskNow);
    // 186..193 GW
    packAsciiToRegs(&mb_netInfoRegs[16], 8, gwNow);
    // 194..201 DNS
    packAsciiToRegs(&mb_netInfoRegs[24], 8, dnsNow);

    // 202 DHCP, 203 TCPPORT
    mb_netInfoRegs[32] = dhcp ? 1 : 0;
    mb_netInfoRegs[33] = port;

    // 204..211 AP_SSID
    packAsciiToRegs(&mb_netInfoRegs[34], 8, apS);
    // 212..219 AP_PASS
    packAsciiToRegs(&mb_netInfoRegs[42], 8, apP);
}

static void updateModbusSettingsRegs() {
    mb_mbSetRegs[0] = g_mbCfg.slaveId;
    mb_mbSetRegs[1] = baudToIndex(g_mbCfg.baud); // INDEX
    mb_mbSetRegs[2] = g_mbCfg.dataBits;
    mb_mbSetRegs[3] = g_mbCfg.parity;
    mb_mbSetRegs[4] = g_mbCfg.stopBits;

    mb_serSetRegs[0] = baudToIndex(g_serCfg.baud); // INDEX
    mb_serSetRegs[1] = g_serCfg.dataBits;
    mb_serSetRegs[2] = g_serCfg.parity;
    mb_serSetRegs[3] = g_serCfg.stopBits;
}

// Add NEW Modbus callbacks
static uint16_t mbSysInfoGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_SYSINFO_START;
    return (idx < 58) ? mb_sysInfoRegs[idx] : 0;
}
static uint16_t mbNetInfoGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_NETINFO_START;
    return (idx < 50) ? mb_netInfoRegs[idx] : 0;
}

// Settings set callbacks
static uint16_t mbMbSetGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_MBSET_START;
    return (idx < 5) ? mb_mbSetRegs[idx] : 0;
}

static uint16_t mbMbSetSet(TRegister* reg, uint16_t val) {
    uint16_t idx = reg->address.address - MB_REG_MBSET_START;
    if (idx >= 5) return reg->value;

    mb_mbSetRegs[idx] = val;

    if (idx == 0) {
        uint8_t sid = (uint8_t)val;
        if (sid < 1 || sid > 247) sid = 1;
        g_mbCfg.slaveId = sid;
    }
    else if (idx == 1) {
        // val is BAUD INDEX
        uint32_t b = indexToBaud(val, 9600);
        g_mbCfg.baud = b;
    }
    else if (idx == 2) {
        g_mbCfg.dataBits = (val == 7) ? 7 : 8;
    }
    else if (idx == 3) {
        g_mbCfg.parity = (val > 2) ? 0 : (uint8_t)val;
    }
    else if (idx == 4) {
        g_mbCfg.stopBits = (val == 2) ? 2 : 1;
    }

    saveModbusConfig();

    reg->value = val;
    return reg->value;
}

static uint16_t mbSerSetGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_SERSET_START;
    return (idx < 4) ? mb_serSetRegs[idx] : 0;
}

static uint16_t mbSerSetSet(TRegister* reg, uint16_t val) {
    uint16_t idx = reg->address.address - MB_REG_SERSET_START;
    if (idx >= 4) return reg->value;

    mb_serSetRegs[idx] = val;

    if (idx == 0) {
        // val is BAUD INDEX
        uint32_t b = indexToBaud(val, 115200);
        g_serCfg.baud = b;
    }
    else if (idx == 1) {
        g_serCfg.dataBits = (val == 7) ? 7 : 8;
    }
    else if (idx == 2) {
        g_serCfg.parity = (val > 2) ? 0 : (uint8_t)val;
    }
    else if (idx == 3) {
        g_serCfg.stopBits = (val == 2) ? 2 : 1;
    }

    saveSerialConfig();

    reg->value = val;
    return reg->value;
}

static uint16_t mbBuzzGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_BUZZ_START;
    return (idx < 6) ? mb_buzRegs[idx] : 0;
}
static uint16_t mbBuzzSet(TRegister* reg, uint16_t val) {
    uint16_t idx = reg->address.address - MB_REG_BUZZ_START;
    if (idx >= 6) return reg->value;

    mb_buzRegs[idx] = val;

    // BUZ_CMD at 255 (idx 5)
    if (idx == 5) {
        uint16_t cmd = val;
        uint16_t freq = mb_buzRegs[0] ? mb_buzRegs[0] : 2000;
        uint16_t ms = mb_buzRegs[1] ? mb_buzRegs[1] : 200;
        uint16_t onMs = mb_buzRegs[2] ? mb_buzRegs[2] : 200;
        uint16_t offMs = mb_buzRegs[3];
        uint8_t rep = (uint8_t)(mb_buzRegs[4] ? mb_buzRegs[4] : 5);

        if (cmd == 1) {
            g_rtc.simpleBeep(freq, ms);
        }
        else if (cmd == 2) {
            buzzStart(freq, onMs, offMs, rep);
        }
        else {
            buzzStop();
            g_rtc.stopBeep();
        }
        // auto-clear cmd
        mb_buzRegs[5] = 0;
    }

    reg->value = val;
    return reg->value;
}



// ======================================================================
// NETCFG serial dump (unchanged except adds AI range)
// ======================================================================
static void netcfgSerialDump() {
    const char* C_RESET = "\x1b[0m";
    const char* C_TITLE = "\x1b[1;36m";
    const char* C_KEY = "\x1b[1;33m";
    const char* C_VAL = "\x1b[1;32m";
    const char* C_WARN = "\x1b[1;31m";

    g_prefs.begin(PREF_NS_NET, true);
    bool apDone = g_prefs.getBool(KEY_AP_DONE, false);
    bool dhcp = g_prefs.getBool(KEY_ETH_DHCP, true);
    String mac = g_prefs.getString(KEY_ETH_MAC, "A8:FD:B5:E1:D4:B3");
    String ipPref = g_prefs.getString(KEY_ETH_IP, "");
    String maskPref = g_prefs.getString(KEY_ETH_MASK, "255.255.255.0");
    String gwPref = g_prefs.getString(KEY_ETH_GW, "");
    String dnsPref = g_prefs.getString(KEY_ETH_DNS, "8.8.8.8");
    uint16_t port = g_prefs.getUShort(KEY_TCP_PORT, 5000);
    String apS = g_prefs.getString(KEY_AP_SSID, g_apSsid);
    String apP = g_prefs.getString(KEY_AP_PASS, g_apPass);
    uint8_t aiVr = (uint8_t)g_prefs.getUChar(KEY_AI_VRANGE, 5);

    String ipCur = g_prefs.getString(KEY_ETH_IP_CUR, "");
    String maskCur = g_prefs.getString(KEY_ETH_MASK_CUR, "");
    String gwCur = g_prefs.getString(KEY_ETH_GW_CUR, "");
    String dnsCur = g_prefs.getString(KEY_ETH_DNS_CUR, "");
    g_prefs.end();

    bool ethReady = g_eth.isReady();
    bool link = g_eth.getLinkStatus();
    String ipNow = ethReady ? ipToString(g_eth.getIP()) : "";
    String maskNow = ethReady ? ipToString(g_eth.getSubnetMask()) : "";
    String gwNow = ethReady ? ipToString(g_eth.getGateway()) : "";
    String dnsNow = ethReady ? ipToString(g_eth.getDNSServer()) : "";

    Serial.println();
    Serial.print(C_TITLE); Serial.println("========== A8R-M NETWORK SETTINGS (SERIAL) =========="); Serial.print(C_RESET);

    Serial.print(C_KEY); Serial.print("AP_DONE"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(apDone ? "YES" : "NO"); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("ETH_MODE"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(dhcp ? "DHCP" : "STATIC"); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("MAC"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(mac); Serial.print(C_RESET);

    Serial.print(C_KEY); Serial.print("AI_VRANGE"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.print((int)aiVr); Serial.println("V"); Serial.print(C_RESET);

    Serial.print(C_TITLE); Serial.println("--- Stored Preferences (next boot) ---"); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("IP"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(ipPref); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("MASK"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(maskPref); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("GW"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(gwPref); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("DNS"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(dnsPref); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("TCPPORT"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(port); Serial.print(C_RESET);

    Serial.print(C_KEY); Serial.print("APSSID"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(apS); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("APPASS"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(apP); Serial.print(C_RESET);

    Serial.print(C_TITLE); Serial.println("--- Last Runtime Lease (saved) ---"); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("IP_CUR"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(ipCur); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("MASK_CUR"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(maskCur); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("GW_CUR"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(gwCur); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("DNS_CUR"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(dnsCur); Serial.print(C_RESET);

    Serial.print(C_TITLE); Serial.println("--- Current Runtime (this session) ---"); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("ETH_READY"); Serial.print(C_RESET); Serial.print(": ");
    Serial.print(C_VAL); Serial.println(ethReady ? "YES" : "NO"); Serial.print(C_RESET);

    Serial.print(C_KEY); Serial.print("LINK"); Serial.print(C_RESET); Serial.print(": ");
    Serial.print(link ? C_VAL : C_WARN); Serial.println(link ? "UP" : "DOWN"); Serial.print(C_RESET);

    Serial.print(C_KEY); Serial.print("IP_NOW"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(ipNow); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("MASK_NOW"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(maskNow); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("GW_NOW"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(gwNow); Serial.print(C_RESET);
    Serial.print(C_KEY); Serial.print("DNS_NOW"); Serial.print(C_RESET); Serial.print(": "); Serial.print(C_VAL); Serial.println(dnsNow); Serial.print(C_RESET);

    Serial.print(C_TITLE); Serial.println("====================================================="); Serial.print(C_RESET);
    Serial.println();
}

// ======================================================================
// SETUP
// ======================================================================
void setup() {
    Serial.begin(115200);
    Debug::begin(115200);
    delay(200);

    hwInfoEnsureDefaults();

    Serial.println();
    Serial.println(F("=== CortexLink A8R-M Controller Firmware (Analog+Multi-Scheduler) ==="));

    loadNetworkConfig();
    loadAnalogConfig();

    // ===== NEW (MODBUS) =====
    loadModbusConfig();

    loadSerialConfig();

    Wire.begin(I2C_SDA_PIN, I2C_SCK_PIN);
    delay(50);

    Debug::infoPrintln("Init Digital Inputs...");
    g_di.begin();
    g_di.setupInterrupts(INPUT_INT_CHANGE);
    g_di.enableRtcSqwInterrupt(true);

    Debug::infoPrintln("Init Relays...");
    g_relays.begin();
    g_relays.turnOffAll();

    Debug::infoPrintln("Init DAC...");
    g_dac.begin(GP8413_DAC_ADDR);

    Debug::infoPrintln("Init Analog Inputs...");
    g_ai.begin();

    Debug::infoPrintln("Init DHT/DS18B20...");
    g_dht.begin();

    Debug::infoPrintln("Init RTC...");
    g_rtc.begin();
    g_rtc.disableSquareWave();

    schedLoadAll();

    g_di.updateAllInputs();
    g_lastDiBits = g_di.getInputState();

    startConfigStation_WiFi();
    stopConfigPortal();
    // WiFi portal decision
    g_prefs.begin(PREF_NS_NET, true);
    bool apDone = g_prefs.getBool(KEY_AP_DONE, false);
    g_prefs.end();

    if (!apDone) {
        Serial.println(F("No AP_DONE flag found -> entering WiFi AP config mode."));
        startConfigPortal();
        g_runConfigPortal = true;
    }
    else {
        Serial.println(F("AP config previously completed -> skipping portal."));
    }

    if (!startEthernet()) {
        Serial.println(F("[FATAL] Ethernet failed to start. Remaining in serial-only mode."));
    }
    else {
        startTcpServer();
        netcfgPersistRuntimeLease();
    }


    // ===== NEW (MODBUS) =====
    if (g_mbCfg.enabled) {
        initModbus();
    }
    else {
        Serial.println(F("MODBUS disabled by preferences."));
    }


    netcfgSerialDump();

    printHelp();
    printStatus();
}

// ======================================================================
// LOOP
// ======================================================================
void loop() {
    g_di.processInterrupts();
    g_rtc.update();
    g_dht.update();
    g_eth.update();

    if (g_runConfigPortal) g_http.handleClient();
    if (g_eth.isReady()) handleTcpClient();

    schedUpdate();
    buzzUpdate();

    // ===== NEW (MODBUS) =====
    if (g_mbCfg.enabled) {
        // refresh Modbus register mirror periodically
        static uint32_t lastMbRefresh = 0;
        if (millis() - lastMbRefresh > 200) {
            lastMbRefresh = millis();
            updateModbusData();
            updateModbusSystemInfoRegs();
            updateModbusNetworkInfoRegs();
            updateModbusSettingsRegs();
        }
        g_modbus.task();
    }

    processSerial();

    if (g_rebootPending && (int32_t)(millis() - g_rebootAtMs) >= 0) {
        tcpSend("NETCFG REBOOTING");
        delay(40);
        ESP.restart();
    }

    static uint32_t lastLeaseSave = 0;
    if (g_eth.isReady() && (millis() - lastLeaseSave) > 5000) {
        lastLeaseSave = millis();
        netcfgPersistRuntimeLease();
    }
}


// ======================================================================
// ===== MODBUS INTEGRATION (based on MODBUS_Test.ino) =====
// ======================================================================

static void initModbus() {
    Serial.println("Initializing MODBUS communication...");

    // Begin with stored baud (ModbusComm::begin only configures serial);
    // parity/data/stop is fixed by HardwareSerial begin config inside ModbusComm currently (8N1).
    // For your required 8N1 defaults, this is correct.
    // If you later want true parity/stopbits control on ESP32, we can extend ModbusComm::begin
    // to accept SERIAL_8N1 / SERIAL_8E1 / SERIAL_8O1, etc.
    if (g_modbus.begin(g_mbCfg.baud)) {
        g_modbus.server(g_mbCfg.slaveId);

        Serial.printf("MODBUS initialized at %lu baud, slaveId=%u\n", g_mbCfg.baud, g_mbCfg.slaveId);
        Serial.println("Configuring MODBUS register handlers...");

        // Input registers (FC 04) 0..7 digital inputs (0/1)
        if (!g_modbus.addInputRegisterHandler(MB_REG_INPUTS_START, NUM_DIGITAL_INPUTS, mbInputsCallback)) {
            ERROR_LOG("Failed to add input register handlers for digital inputs");
        }

        // Discrete inputs (FC 02) 0..7: same digital inputs (0/1)
        if (!g_modbus.addDiscreteInputHandler(MB_REG_INPUTS_START, NUM_DIGITAL_INPUTS, mbDiscreteInputsCallback)) {
            ERROR_LOG("Failed to add discrete input handlers for digital inputs");
        }

        // Holding registers for relays (10..15)
        if (!g_modbus.addHoldingRegisterHandlers(MB_REG_RELAYS_START, NUM_RELAY_OUTPUTS, mbRelaysGet, mbRelaysSet)) {
            ERROR_LOG("Failed to add holding register handlers for relays");
        }

        // Coils for relays (10..15) with separate GET/SET callbacks
        if (!g_modbus.addCoilHandlers(MB_REG_RELAYS_START, NUM_RELAY_OUTPUTS, mbRelaysCoilGet, mbRelaysCoilSet)) {
            ERROR_LOG("Failed to add coil handlers for relays");
        }

        // Input registers for analog + current (from 20)
        const uint16_t analogBlockRegs = (NUM_ANALOG_CHANNELS + NUM_CURRENT_CHANNELS) * 2; // raw+scaled per channel
        if (!g_modbus.addInputRegisterHandler(MB_REG_ANALOG_START, analogBlockRegs, mbAnalogCallback)) {
            ERROR_LOG("Failed to add input register handlers for analog block");
        }

        // DHT temp (30..) and humidity (40..)
        if (!g_modbus.addInputRegisterHandler(MB_REG_TEMP_START, NUM_DHT_SENSORS,
            [](TRegister* reg, uint16_t) {
                return mb_tempRegs[reg->address.address - MB_REG_TEMP_START];
            })) {
            ERROR_LOG("Failed to add input register handlers for temperature");
        }
        if (!g_modbus.addInputRegisterHandler(MB_REG_HUM_START, NUM_DHT_SENSORS,
            [](TRegister* reg, uint16_t) {
                return mb_humRegs[reg->address.address - MB_REG_HUM_START];
            })) {
            ERROR_LOG("Failed to add input register handlers for humidity");
        }

        // DS18B20 temps (50..)
        if (!g_modbus.addInputRegisterHandler(MB_REG_DS18B20_START, MAX_DS18B20_SENSORS,
            [](TRegister* reg, uint16_t) {
                return mb_ds18Regs[reg->address.address - MB_REG_DS18B20_START];
            })) {
            ERROR_LOG("Failed to add input register handlers for DS18B20");
        }

        // DAC controls (70..73) as holding registers
        if (!g_modbus.addHoldingRegisterHandlers(MB_REG_DAC_START, 4, mbDacGet, mbDacSet)) {
            ERROR_LOG("Failed to add holding register handlers for DAC");
        }

        // RTC (80..87) as holding registers
        if (!g_modbus.addHoldingRegisterHandlers(MB_REG_RTC_START, 8, mbRtcGet, mbRtcSet)) {
            ERROR_LOG("Failed to add holding register handlers for RTC");
        }


        // ===== NEW: System/Info (BoardInfo) holding regs 100..158 =====
        refreshSysInfoRegs();
        if (!g_modbus.addHoldingRegisterHandler(MB_REG_SYSINFO_START, 58, mbSysInfoGet)) {
            ERROR_LOG("Failed to add BoardInfo holding registers");
        }

        // ===== NEW: NetworkInfo holding regs 170..219 =====
        refreshNetInfoRegs();
        if (!g_modbus.addHoldingRegisterHandlers(MB_REG_NETINFO_START, 50, mbNetInfoGet, mbNetInfoSet)) {
            ERROR_LOG("Failed to add NetworkInfo holding registers");
        }

        // ===== NEW: Modbus settings 230..234 =====
        refreshMbSettingsRegs();
        if (!g_modbus.addHoldingRegisterHandlers(MB_REG_MBSET_START, 5, mbMbSetGet, mbMbSetSet)) {
            ERROR_LOG("Failed to add Modbus settings holding registers");
        }

        // ===== NEW: Serial (USB console) settings 240..243 =====
        refreshSerialSettingsRegs();
        if (!g_modbus.addHoldingRegisterHandlers(MB_REG_SERSET_START, 4, mbSerSetGet, mbSerSetSet)) {
            ERROR_LOG("Failed to add Serial settings holding registers");
        }

        // ===== NEW: Buzzer control 250..255 =====
        refreshBuzzerRegsDefaults();
        if (!g_modbus.addHoldingRegisterHandlers(MB_REG_BUZZ_START, 6, mbBuzzGet, mbBuzzSet)) {
            ERROR_LOG("Failed to add Buzzer holding registers");
        }

        // ===== NEW: MAC info holding regs 290..316 =====
        refreshMacInfoRegs();
        if (!g_modbus.addHoldingRegisterHandler(MB_REG_MACINFO_START, MB_MACINFO_REGS, mbMacInfoGet)) {
            ERROR_LOG("Failed to add MACINFO holding registers");
        }

        Serial.println("MODBUS register handlers configured");
    }
    else {
        Serial.println("Failed to initialize MODBUS");
    }
}

static void updateModbusData() {
    // Digital inputs
    if (g_di.isConnected()) {
        g_di.updateAllInputs();
        uint8_t st = g_di.getInputState();
        for (int i = 0; i < NUM_DIGITAL_INPUTS; i++) mb_inputRegs[i] = (st & (1 << i)) ? 1 : 0;
    }
    else {
        for (int i = 0; i < NUM_DIGITAL_INPUTS; i++) mb_inputRegs[i] = 0;
    }

    // Relay states (store logical 0/1 where 1=ON, 0=OFF)
    // NOTE: Your RelayDriver uses active-low. getAllStates returns GPIO_A bits; ON means bit=0.
    if (g_relays.isConnected()) {
        uint8_t relBits = g_relays.getAllStates() & 0x3F;
        for (int i = 0; i < NUM_RELAY_OUTPUTS; i++) {
            bool on = ((relBits & (1 << i)) == 0); // active-low ON
            mb_relayRegs[i] = on ? 1 : 0;
        }
    }
    else {
        for (int i = 0; i < NUM_RELAY_OUTPUTS; i++) mb_relayRegs[i] = 0;
    }

    // Analog voltage: raw + scaled*100
    for (uint8_t i = 0; i < NUM_ANALOG_CHANNELS; i++) {
        mb_analogRegs[i * 2] = g_ai.readRawVoltageChannel(i);
        mb_analogRegs[i * 2 + 1] = (uint16_t)lroundf(g_ai.readVoltage(i) * 100.0f);
    }

    // Analog current: raw + scaled*100
    for (uint8_t i = 0; i < NUM_CURRENT_CHANNELS; i++) {
        const uint8_t base = (NUM_ANALOG_CHANNELS + i) * 2;
        mb_analogRegs[base] = g_ai.readRawCurrentChannel(i);
        mb_analogRegs[base + 1] = (uint16_t)lroundf(g_ai.readCurrent(i) * 100.0f);
    }

    // DHT / DS18
    g_dht.update();
    for (uint8_t i = 0; i < NUM_DHT_SENSORS; i++) {
        float t = g_dht.getTemperature(i); // already in current unit; firmware uses Celsius by default
        float h = g_dht.getHumidity(i);
        mb_tempRegs[i] = isnan(t) ? 0 : (uint16_t)lroundf(t * 100.0f);
        mb_humRegs[i] = isnan(h) ? 0 : (uint16_t)lroundf(h * 100.0f);
    }
    uint8_t dsCnt = g_dht.getDS18B20Count();
    for (uint8_t i = 0; i < MAX_DS18B20_SENSORS; i++) mb_ds18Regs[i] = 0;
    for (uint8_t i = 0; i < dsCnt && i < MAX_DS18B20_SENSORS; i++) {
        float t = g_dht.getDS18B20Temperature(i);
        // Keep same encoding as MODBUS_Test: (t + 100.0) * 100, 0 means invalid
        mb_ds18Regs[i] = (t < -100.0f) ? 0 : (uint16_t)lroundf((t + 100.0f) * 100.0f);
    }

    // RTC
    if (g_rtc.isConnected()) {
        DateTime now = g_rtc.getDateTime(true);
        mb_rtcRegs[0] = now.year();
        mb_rtcRegs[1] = now.month();
        mb_rtcRegs[2] = now.day();
        mb_rtcRegs[3] = now.hour();
        mb_rtcRegs[4] = now.minute();
        mb_rtcRegs[5] = now.second();
    }

    // DAC
    if (g_dac.isInitialized()) {
        mb_dacRegs[0] = g_dac.getRaw(0);
        mb_dacRegs[1] = g_dac.getRaw(1);
        mb_dacRegs[2] = (uint16_t)lroundf(g_dac.getVoltage(0) * 100.0f);
        mb_dacRegs[3] = (uint16_t)lroundf(g_dac.getVoltage(1) * 100.0f);
    }


    // Refresh dynamic sys/net mirrors periodically (cheap)
    refreshSysInfoRegs();
    refreshNetInfoRegs();
    refreshMbSettingsRegs();
    refreshSerialSettingsRegs();
    refreshMacInfoRegs();
}

// Input registers (FC04)
static uint16_t mbInputsCallback(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_INPUTS_START;
    return (idx < NUM_DIGITAL_INPUTS) ? mb_inputRegs[idx] : 0;
}

// Discrete inputs (FC02) callback: return 0xFF00 / 0x0000
static uint16_t mbDiscreteInputsCallback(TRegister* reg, uint16_t) {
    const uint16_t idx = reg->address.address - MB_REG_INPUTS_START;
    if (idx >= NUM_DIGITAL_INPUTS) {
        reg->value = 0x0000;
        return reg->value;
    }
    const bool on = (mb_inputRegs[idx] != 0);
    reg->value = on ? 0xFF00 : 0x0000;
    return reg->value;
}

// Holding relays (FC03/FC06)
static inline bool isRelayOnValue(uint16_t v) {
    return (v == 0x0001) || (v == 0x00FF) || (v == 0xFF00) || (v == 0xFFFF);
}
static inline bool isRelayOffValue(uint16_t v) {
    return (v == 0x0000);
}

static uint16_t mbRelaysGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_RELAYS_START;
    uint16_t val = (idx < NUM_RELAY_OUTPUTS) ? mb_relayRegs[idx] : 0;
    return val;
}

static uint16_t mbRelaysSet(TRegister* reg, uint16_t val) {
    const uint16_t addr = reg->address.address;
    const uint16_t idx = addr - MB_REG_RELAYS_START;
    if (idx >= NUM_RELAY_OUTPUTS) {
        reg->value = val;
        return 0;
    }

    const uint16_t prev = mb_relayRegs[idx];
    bool desiredState = (prev != 0);
    bool validPattern = true;

    if (isRelayOffValue(val)) desiredState = false;
    else if (isRelayOnValue(val)) desiredState = true;
    else {
        // ignore invalid pattern (no toggle here, to keep stable)
        validPattern = false;
    }

    uint16_t desired = desiredState ? 1 : 0;

    if (desired != prev) {
        if (g_relays.isConnected()) {
            g_relays.setState((uint8_t)(idx + 1), desired ? RELAY_ON : RELAY_OFF);
        }
        mb_relayRegs[idx] = desired;
    }

    // Echo back raw value per FC06
    reg->value = val;
    (void)validPattern;
    return desired;
}

// Coils (FC01/FC05)
static inline uint16_t coilStateToRaw(uint16_t state01) {
    return state01 ? 0xFF00 : 0x0000;
}

static uint16_t mbRelaysCoilGet(TRegister* reg, uint16_t) {
    const uint16_t addr = reg->address.address;
    const uint16_t idx = addr - MB_REG_RELAYS_START;
    if (idx >= NUM_RELAY_OUTPUTS) return 0x0000;

    const uint16_t raw = coilStateToRaw(mb_relayRegs[idx]);
    reg->value = raw;
    return raw;
}

static uint16_t mbRelaysCoilSet(TRegister* reg, uint16_t val) {
    const uint16_t addr = reg->address.address;
    const uint16_t idx = addr - MB_REG_RELAYS_START;
    if (idx >= NUM_RELAY_OUTPUTS) return reg->value;

    const uint16_t prev = mb_relayRegs[idx];
    uint16_t desired = prev;

    if (val == 0xFF00) desired = 1;
    else if (val == 0x0000) desired = 0;
    else {
        // Non-standard => ignore (no toggle). PC should send FF00/0000.
    }

    if (desired != prev) {
        if (g_relays.isConnected()) {
            g_relays.setState((uint8_t)(idx + 1), desired ? RELAY_ON : RELAY_OFF);
        }
        mb_relayRegs[idx] = desired;
    }

    reg->value = val; // echo request raw
    return val;
}

// Analog input registers (FC04)
static uint16_t mbAnalogCallback(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_ANALOG_START;
    const uint16_t total = (NUM_ANALOG_CHANNELS + NUM_CURRENT_CHANNELS) * 2;
    return (idx < total) ? mb_analogRegs[idx] : 0;
}

// DAC holding regs (FC03/FC06)
static uint16_t mbDacGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_DAC_START;
    return (idx < 4) ? mb_dacRegs[idx] : 0;
}

static uint16_t mbDacSet(TRegister* reg, uint16_t val) {
    uint16_t idx = reg->address.address - MB_REG_DAC_START;
    if (idx >= 4) return reg->value;

    mb_dacRegs[idx] = val;

    if (g_dac.isInitialized()) {
        switch (idx) {
        case 0: g_dac.setRaw(0, val); break;
        case 1: g_dac.setRaw(1, val); break;
        case 2: g_dac.setVoltage(0, val / 100.0f); break;
        case 3: g_dac.setVoltage(1, val / 100.0f); break;
        }
    }

    reg->value = val;
    return reg->value;
}

// RTC holding regs (FC03/FC06)
static uint16_t mbRtcGet(TRegister* reg, uint16_t) {
    uint16_t idx = reg->address.address - MB_REG_RTC_START;
    return (idx < 8) ? mb_rtcRegs[idx] : 0;
}

static uint16_t mbRtcSet(TRegister* reg, uint16_t val) {
    uint16_t idx = reg->address.address - MB_REG_RTC_START;
    if (idx >= 8) return reg->value;

    mb_rtcRegs[idx] = val;
    reg->value = val;

    // Apply RTC only when seconds register written (idx 5), like MODBUS_Test
    if (idx == 5 && g_rtc.isConnected()) {
        g_rtc.setDateTime(DateTime(
            mb_rtcRegs[0], mb_rtcRegs[1], mb_rtcRegs[2],
            mb_rtcRegs[3], mb_rtcRegs[4], mb_rtcRegs[5]
        ));
    }
    return reg->value;
}



// ======================================================================
// NETWORK CONFIG LOAD
// ======================================================================
void loadNetworkConfig() {
    g_prefs.begin(PREF_NS_NET, true);
    bool apDone = g_prefs.getBool(KEY_AP_DONE, false);
    bool dhcp = g_prefs.getBool(KEY_ETH_DHCP, true);
    String mac = g_prefs.getString(KEY_ETH_MAC, "A8:FD:B5:E1:D4:B3");
    String ip = g_prefs.getString(KEY_ETH_IP, "");
    String mask = g_prefs.getString(KEY_ETH_MASK, "255.255.255.0");
    String gw = g_prefs.getString(KEY_ETH_GW, "");
    String dns = g_prefs.getString(KEY_ETH_DNS, "8.8.8.8");
    uint16_t port = g_prefs.getUShort(KEY_TCP_PORT, 5000);

    String apS = g_prefs.getString(KEY_AP_SSID, "A8RM-SETUP");
    String apP = g_prefs.getString(KEY_AP_PASS, "cortexlink");
    g_prefs.end();

    g_tcpPort = port;
    g_apSsid = apS;
    g_apPass = apP;

    Serial.println(F("Loaded network config:"));
    Serial.print(F("  AP_DONE: ")); Serial.println(apDone ? "YES" : "NO");
    Serial.print(F("  ETH_DHCP: ")); Serial.println(dhcp ? "YES" : "NO");
    Serial.print(F("  MAC: ")); Serial.println(mac);
    Serial.print(F("  IP: ")); Serial.println(ip);
    Serial.print(F("  MASK: ")); Serial.println(mask);
    Serial.print(F("  GW: ")); Serial.println(gw);
    Serial.print(F("  DNS: ")); Serial.println(dns);
    Serial.print(F("  TCP PORT: ")); Serial.println(g_tcpPort);
    Serial.print(F("  AP SSID: ")); Serial.println(g_apSsid);
    Serial.print(F("  AP PASS: ")); Serial.println(g_apPass);
}

// ======================================================================
// WIFI AP CONFIG PORTAL (unchanged; not extended here)
// ======================================================================
void startConfigPortal() {

    uint8_t mac[6];

    Serial.println(F("\n=== Starting WiFi AP Config Portal ==="));
    loadNetworkConfig();

    if (!parseMacString(AP_MAC, mac))
    {
        Serial.println("Invalid AP MAC format");
        return;
    }

    WiFi.mode(WIFI_AP);

    /* Set AP MAC address */
    esp_err_t err = esp_wifi_set_mac(WIFI_IF_AP, mac);
    if (err == ESP_OK)
        Serial.println("AP MAC address set successfully");
    else
        Serial.printf("Failed to set AP MAC, error: %d\n", err);

    /* Store AP MAC in flash (NVS) */
    g_prefs.begin("wifi_cfg", false);
    g_prefs.putString("ap_mac", AP_MAC);
    g_prefs.end();

    Serial.println("AP MAC stored in flash");

    WiFi.softAP(g_apSsid.c_str(), g_apPass.c_str());
    IPAddress apIp = WiFi.softAPIP();
    Serial.print(F("AP SSID: ")); Serial.println(g_apSsid);
    Serial.print(F("AP PASS: ")); Serial.println(g_apPass);
    Serial.print(F("AP IP  : ")); Serial.println(apIp);

    g_http.on("/", handleHttpRoot);
    g_http.on("/api/get", handleHttpGetConfig);
    g_http.on("/api/set", HTTP_POST, handleHttpSetConfig);
    g_http.onNotFound(handleHttpNotFound);
    g_http.begin();
}

void startConfigStation_WiFi() {
    uint8_t mac[6];


    if (!parseMacString(STA_MAC, mac))
    {
        Serial.println("Invalid MAC format");
        return;
    }

    WiFi.mode(WIFI_STA);   // MUST be before setting MAC

    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, mac);
    if (err == ESP_OK)
        Serial.println("STA MAC address set successfully");
    else
        Serial.printf("Failed to set MAC, error: %d\n", err);

    g_prefs.begin("wifi_cfg", false);
    g_prefs.putString("sta_mac", STA_MAC);
    g_prefs.end();

    g_prefs.begin("wifi_cfg", true);
    String storedMac = g_prefs.getString("sta_mac", "00:00:00:00:00:00");
    g_prefs.end();

    Serial.print("MAC read from flash: ");
    Serial.println(storedMac);

    uint8_t hwMac[6];
    esp_wifi_get_mac(WIFI_IF_STA, hwMac);

    Serial.print("Current STA MAC: ");
    for (int i = 0; i < 6; i++)
    {
        if (hwMac[i] < 0x10) Serial.print("0");
        Serial.print(hwMac[i], HEX);
        if (i < 5) Serial.print(":");
    }
    Serial.println();

}



void stopConfigPortal() {
    g_http.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    g_runConfigPortal = false;
    Serial.println(F("Config portal stopped, WiFi disabled."));
}
void handleHttpRoot() { g_http.send(200, "text/plain", "Portal UI not included in this build."); }
void handleHttpGetConfig() { g_http.send(200, "application/json", "{}"); }
void handleHttpSetConfig() { g_http.send(200, "text/plain", "OK"); }
void handleHttpNotFound() { g_http.send(404, "text/plain", "Not found"); }

// ======================================================================
// ETHERNET BRING-UP
// ======================================================================
bool startEthernet() {
    Serial.println(F("\n=== Starting W5500 Ethernet ==="));
    g_prefs.begin(PREF_NS_NET, true);
    bool dhcp = g_prefs.getBool(KEY_ETH_DHCP, true);
    String macS = g_prefs.getString(KEY_ETH_MAC, "A8:FD:B5:E1:D4:B3");
    String ipS = g_prefs.getString(KEY_ETH_IP, "");
    String maskS = g_prefs.getString(KEY_ETH_MASK, "255.255.255.0");
    String gwS = g_prefs.getString(KEY_ETH_GW, "");
    String dnsS = g_prefs.getString(KEY_ETH_DNS, "8.8.8.8");
    g_prefs.end();

    bool ok = false;
    if (dhcp) {
        Serial.println(F("Ethernet: DHCP mode"));
        ok = g_eth.beginDHCP(macS);
    }
    else {
        Serial.println(F("Ethernet: STATIC mode"));
        ok = g_eth.begin(macS, ipS, dnsS, gwS, maskS);
    }
    if (!ok) { Serial.println(F("Ethernet failed to initialize.")); return false; }

    Serial.print(F("Ethernet ready, IP="));
    Serial.println(g_eth.getIP());
    return true;
}

void startTcpServer() {
    if (g_tcpServer) { delete g_tcpServer; g_tcpServer = nullptr; }
    g_tcpServer = new EthernetServer(g_tcpPort);
    g_tcpServer->begin();
    Serial.print(F("TCP control server started on port "));
    Serial.println(g_tcpPort);
}

// ======================================================================
// TCP CLIENT HANDLING
// ======================================================================
void handleTcpClient() {
    if (!g_eth.isReady() || !g_tcpServer) return;

    if (!g_tcpClient || !g_tcpClient.connected()) {
        g_tcpClient.stop();
        g_tcpClient = g_tcpServer->available();
        if (g_tcpClient) {
            g_lastClientRx = millis();
            g_tcpClient.setTimeout(100);
            g_tcpClient.println(F("HELLO A8R-M"));
            sendSnapshot(g_tcpClient, true);
        }
        return;
    }

    if (millis() - g_lastClientRx > CLIENT_TIMEOUT_MS) {
        g_tcpClient.stop();
        return;
    }

    while (g_tcpClient && g_tcpClient.connected() && g_tcpClient.available()) {
        String line = g_tcpClient.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        g_lastClientRx = millis();
        handleTcpCommand(line);
    }
}

// ======================================================================
// DAC RANGE NOTE
// ======================================================================
static void applyScheduleDacRange(const ScheduleConfig& s) {
    if (!g_dac.isInitialized()) return;
    uint8_t r = 5;
    if (s.dac1Range == 10 || s.dac2Range == 10) r = 10;
    g_dac.setOutputRange(r == 10 ? RANGE_0_10V : RANGE_0_5V);
}

// ======================================================================
// NETCFG TCP PROTOCOL
// + includes AI_VRANGE setting and runtime IP_NOW
// ======================================================================
static void netcfgReplyGet() {
    g_prefs.begin(PREF_NS_NET, true);
    bool dhcp = g_prefs.getBool(KEY_ETH_DHCP, true);
    String mac = g_prefs.getString(KEY_ETH_MAC, "A8:FD:B5:E1:D4:B3");
    String ip = g_prefs.getString(KEY_ETH_IP, "");
    String mask = g_prefs.getString(KEY_ETH_MASK, "255.255.255.0");
    String gw = g_prefs.getString(KEY_ETH_GW, "");
    String dns = g_prefs.getString(KEY_ETH_DNS, "8.8.8.8");
    uint16_t port = g_prefs.getUShort(KEY_TCP_PORT, 5000);
    String apS = g_prefs.getString(KEY_AP_SSID, g_apSsid);
    String apP = g_prefs.getString(KEY_AP_PASS, g_apPass);
    uint8_t aiVr = (uint8_t)g_prefs.getUChar(KEY_AI_VRANGE, 5);
    g_prefs.end();

    bool link = g_eth.getLinkStatus();
    IPAddress ipNow = g_eth.isReady() ? g_eth.getIP() : IPAddress(0, 0, 0, 0);
    IPAddress maskNow = g_eth.isReady() ? g_eth.getSubnetMask() : IPAddress(0, 0, 0, 0);
    IPAddress gwNow = g_eth.isReady() ? g_eth.getGateway() : IPAddress(0, 0, 0, 0);
    IPAddress dnsNow = g_eth.isReady() ? g_eth.getDNSServer() : IPAddress(0, 0, 0, 0);

    String out = "NETCFG ";
    out += "MAC=" + mac;
    out += " DHCP=" + String(dhcp ? 1 : 0);
    out += " IP=" + ip;
    out += " MASK=" + mask;
    out += " GW=" + gw;
    out += " DNS=" + dns;
    out += " TCPPORT=" + String(port);
    out += " APSSID=" + apS;
    out += " APPASS=" + apP;
    out += " AI_VRANGE=" + String((aiVr == 10) ? 10 : 5);

    out += " CLIENTIP=" + ipToString(ipNow);
    out += " LINK=" + String(link ? "UP" : "DOWN");

    out += " IP_NOW=" + ipToString(ipNow);
    out += " MASK_NOW=" + ipToString(maskNow);
    out += " GW_NOW=" + ipToString(gwNow);
    out += " DNS_NOW=" + ipToString(dnsNow);

    tcpSend(out);
}

static void netcfgApplySet(const String& args) {
    g_prefs.begin(PREF_NS_NET, true);
    bool dhcp = g_prefs.getBool(KEY_ETH_DHCP, true);
    String mac = g_prefs.getString(KEY_ETH_MAC, "A8:FD:B5:E1:D4:B3");
    String ip = g_prefs.getString(KEY_ETH_IP, "");
    String mask = g_prefs.getString(KEY_ETH_MASK, "255.255.255.0");
    String gw = g_prefs.getString(KEY_ETH_GW, "");
    String dns = g_prefs.getString(KEY_ETH_DNS, "8.8.8.8");
    uint16_t port = g_prefs.getUShort(KEY_TCP_PORT, 5000);
    String apS = g_prefs.getString(KEY_AP_SSID, g_apSsid);
    String apP = g_prefs.getString(KEY_AP_PASS, g_apPass);
    uint8_t aiVr = (uint8_t)g_prefs.getUChar(KEY_AI_VRANGE, 5);
    g_prefs.end();

    String sDhcp = getTokValue(args, "DHCP");
    String sMac = getTokValue(args, "MAC");
    String sIp = getTokValue(args, "IP");
    String sMask = getTokValue(args, "MASK");
    String sGw = getTokValue(args, "GW");
    String sDns = getTokValue(args, "DNS");
    String sPort = getTokValue(args, "TCPPORT");
    String sApSsid = getTokValue(args, "APSSID");
    String sApPass = getTokValue(args, "APPASS");
    String sReboot = getTokValue(args, "REBOOT");
    String sAiVr = getTokValue(args, "AI_VRANGE");

    if (sDhcp.length()) dhcp = (sDhcp.toInt() != 0);
    if (sMac.length()) mac = sMac;
    if (sIp.length()) ip = sIp;
    if (sMask.length()) mask = sMask;
    if (sGw.length()) gw = sGw;
    if (sDns.length()) dns = sDns;
    if (sPort.length()) {
        long p = sPort.toInt();
        if (p < 1) p = 1;
        if (p > 65535) p = 65535;
        port = (uint16_t)p;
    }
    if (sApSsid.length()) apS = sApSsid;
    if (sApPass.length()) apP = sApPass;
    if (sAiVr.length()) {
        int vr = sAiVr.toInt();
        aiVr = (uint8_t)((vr == 10) ? 10 : 5);
    }

    if (!netcfgIsSafeToken(mac) || !netcfgIsSafeToken(ip) || !netcfgIsSafeToken(mask) ||
        !netcfgIsSafeToken(gw) || !netcfgIsSafeToken(dns) || !netcfgIsSafeToken(apS) || !netcfgIsSafeToken(apP)) {
        tcpSend("NETCFG ERR TOKENS");
        return;
    }

    g_prefs.begin(PREF_NS_NET, false);
    g_prefs.putBool(KEY_ETH_DHCP, dhcp);
    g_prefs.putString(KEY_ETH_MAC, mac);
    g_prefs.putString(KEY_ETH_IP, ip);
    g_prefs.putString(KEY_ETH_MASK, mask);
    g_prefs.putString(KEY_ETH_GW, gw);
    g_prefs.putString(KEY_ETH_DNS, dns);
    g_prefs.putUShort(KEY_TCP_PORT, port);
    g_prefs.putString(KEY_AP_SSID, apS);
    g_prefs.putString(KEY_AP_PASS, apP);
    g_prefs.putUChar(KEY_AI_VRANGE, aiVr);
    g_prefs.end();

    g_tcpPort = port;
    g_apSsid = apS;
    g_apPass = apP;
    saveAnalogConfig(aiVr);

    tcpSend("NETCFG OK");

    const bool doReboot = (sReboot.toInt() != 0);
    if (doReboot) { netcfgScheduleReboot(600); return; }

    startTcpServer();
}

// ======================================================================
// Analog trigger evaluation helpers
// ======================================================================
static bool analogRawCondition(const AnalogTriggerConfig& a) {
    if (!a.enabled) return false;

    uint8_t ch0 = (a.ch < 1) ? 0 : ((a.ch > 2) ? 1 : (a.ch - 1));

    int32_t val = 0;
    if (a.type == A_TYPE_VOLT) val = g_ai.readVoltage_mV(ch0);
    else                      val = g_ai.readCurrent_mA(ch0);

    switch (a.op) {
    case A_OP_ABOVE:
        return val > a.v1;
    case A_OP_BELOW:
        return val < a.v1;
    case A_OP_EQUAL:
        return (abs(val - a.v1) <= a.tol);
    case A_OP_IN_RANGE: {
        int32_t lo = min(a.v1, a.v2);
        int32_t hi = max(a.v1, a.v2);
        return (val >= lo && val <= hi);
    }
    default:
        return false;
    }
}

// Hysteresis: implement as "stateful Schmitt" around threshold(s)
static bool analogConditionWithHysteresis(const AnalogTriggerConfig& a, bool prevStable) {
    if (!a.enabled) return false;

    uint8_t ch0 = (a.ch < 1) ? 0 : ((a.ch > 2) ? 1 : (a.ch - 1));
    int32_t val = (a.type == A_TYPE_VOLT) ? g_ai.readVoltage_mV(ch0) : g_ai.readCurrent_mA(ch0);

    int32_t h = max<int32_t>(0, a.hysteresis);

    switch (a.op) {
    case A_OP_ABOVE: {
        int32_t th = a.v1;
        if (!prevStable) return val > (th + h);
        else             return val > (th - h);
    }
    case A_OP_BELOW: {
        int32_t th = a.v1;
        if (!prevStable) return val < (th - h);
        else             return val < (th + h);
    }
    case A_OP_EQUAL: {
        // For EQUAL, we keep tolerance and optionally widen it with hysteresis
        int32_t tol = max<int32_t>(0, a.tol);
        int32_t tolH = tol + h;
        return (abs(val - a.v1) <= tolH);
    }
    case A_OP_IN_RANGE: {
        int32_t lo = min(a.v1, a.v2);
        int32_t hi = max(a.v1, a.v2);
        if (!prevStable) {
            return (val >= (lo + h) && val <= (hi - h));
        }
        else {
            return (val >= (lo - h) && val <= (hi + h));
        }
    }
    default:
        return false;
    }
}

static bool analogStableConditionUpdate(AnalogTriggerConfig& a) {
    // Returns true if stable condition changed
    bool raw = analogConditionWithHysteresis(a, a.condition);

    uint32_t now = millis();
    if (raw != a.pending) {
        a.pending = raw;
        a.pendingSinceMs = now;
    }

    uint16_t db = (a.debounceMs < 10) ? 10 : a.debounceMs;
    if (a.condition != a.pending) {
        if (now - a.pendingSinceMs >= db) {
            a.condition = a.pending;
            return true;
        }
    }
    return false;
}

// ======================================================================
// TCP COMMAND HANDLER
// ======================================================================
void handleTcpCommand(const String& line) {
    String cmd = line;
    cmd.trim();
    if (cmd.length() == 0) return;

    int sp = cmd.indexOf(' ');
    String op = (sp < 0) ? cmd : cmd.substring(0, sp);
    op.toUpperCase();
    String args = (sp < 0) ? "" : cmd.substring(sp + 1);

    if (op == "PING") { tcpSend("PONG"); return; }

    if (op == "SNAPSHOT") { if (g_tcpClient && g_tcpClient.connected()) sendSnapshot(g_tcpClient, false); return; }
    if (op == "SNAPJSON") { if (g_tcpClient && g_tcpClient.connected()) sendSnapshot(g_tcpClient, true);  return; }

    if (op == "NETCFG") {
        String sub = args; sub.trim();
        int sp2 = sub.indexOf(' ');
        String sop = (sp2 < 0) ? sub : sub.substring(0, sp2);
        sop.toUpperCase();
        String sargs = (sp2 < 0) ? "" : sub.substring(sp2 + 1);

        if (sop == "GET") { netcfgReplyGet(); return; }
        if (sop == "SET") { netcfgApplySet(sargs); return; }
        tcpSend("NETCFG ERR");
        return;
    }

    if (op == "SERIALNET") {
        netcfgSerialDump();
        tcpSend("SERIALNET OK");
        return;
    }

    if (op == "REL") {
        int sp1 = args.indexOf(' ');
        if (sp1 < 0) { tcpSend("ERR REL"); return; }
        uint8_t ch = args.substring(0, sp1).toInt();
        String v = args.substring(sp1 + 1); v.toUpperCase();
        bool ok = false;
        if (ch >= 1 && ch <= NUM_RELAY_OUTPUTS && g_relays.isConnected()) {
            if (v == "1" || v == "ON") ok = g_relays.turnOn(ch);
            else if (v == "0" || v == "OFF") ok = g_relays.turnOff(ch);
            else if (v == "TOGGLE" || v == "TG") ok = g_relays.toggle(ch);
        }
        tcpSend(String("REL ") + String(ch) + " " + (ok ? "OK" : "FAIL"));
        return;
    }

    if (op == "DACV") {
        int sp1 = args.indexOf(' ');
        if (sp1 < 0) { tcpSend("ERR DACV"); return; }
        uint8_t ch = args.substring(0, sp1).toInt();
        int mv = args.substring(sp1 + 1).toInt();
        if (ch < 1 || ch > 2 || !g_dac.isInitialized()) { tcpSend("ERR DACV"); return; }
        g_dac.setVoltage(ch - 1, mv / 1000.0f);
        tcpSend("DACV OK");
        return;
    }

    if (op == "BEEP") {
        int sp1 = args.indexOf(' ');
        if (sp1 < 0) { tcpSend("ERR BEEP"); return; }
        uint16_t freq = (uint16_t)args.substring(0, sp1).toInt();
        uint32_t ms = (uint32_t)args.substring(sp1 + 1).toInt();
        g_rtc.simpleBeep(freq, ms);
        tcpSend("BEEP OK");
        return;
    }

    if (op == "BUZZOFF") {
        buzzStop();
        g_rtc.stopBeep();
        tcpSend("BUZZOFF OK");
        return;
    }

    if (op == "RTCGET") {
        DateTime now = g_rtc.getDateTime(true);
        char buf[32];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
            now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
        tcpSend(String("RTC ") + buf);
        return;
    }

    if (op == "RTCSET") {
        if (g_rtc.setDateTimeFromString(args.c_str())) tcpSend("RTCSET OK");
        else tcpSend("RTCSET ERR");
        return;
    }

    if (op == "NET?") {
        IPAddress ip = g_eth.getIP();
        bool link = g_eth.getLinkStatus();
        tcpSend(String("NET IP=") + ipToString(ip) + " LINK=" + (link ? "UP" : "DOWN"));
        return;
    }

    if (op == "BOARDINFO") {
        hwInfoPrintBoardInfoTcp();
        return;
    }

    // ==================================================================
    // SCHED protocol (extended with analog trigger)
    // ==================================================================
    if (op == "SCHED") {
        String sub = args; sub.trim();
        int sp2 = sub.indexOf(' ');
        String sop = (sp2 < 0) ? sub : sub.substring(0, sp2);
        sop.toUpperCase();
        String sargs = (sp2 < 0) ? "" : sub.substring(sp2 + 1);

        auto parseInto = [&](ScheduleConfig& s, const String& t) {
            String enS = getTokValue(t, "EN");
            String modeS = getTokValue(t, "MODE"); modeS.toUpperCase();
            String diS = getTokValue(t, "DI");
            String edgeS = getTokValue(t, "EDGE"); edgeS.toUpperCase();
            String startS = getTokValue(t, "START");
            String endS = getTokValue(t, "END");
            String recurS = getTokValue(t, "RECUR");
            String daysS = getTokValue(t, "DAYS");

            String rmaskS = getTokValue(t, "RELMASK");
            String ractS = getTokValue(t, "RELACT"); ractS.toUpperCase();

            String d1mvS = getTokValue(t, "DAC1MV");
            String d2mvS = getTokValue(t, "DAC2MV");
            String d1rS = getTokValue(t, "DAC1R");
            String d2rS = getTokValue(t, "DAC2R");

            String buzenS = getTokValue(t, "BUZEN");
            String buzfS = getTokValue(t, "BUZFREQ");
            String buzonS = getTokValue(t, "BUZON");
            String buzoffS = getTokValue(t, "BUZOFF");
            String buzrepS = getTokValue(t, "BUZREP");

            // NEW Analog tokens
            String aenS = getTokValue(t, "AEN");
            String atyS = getTokValue(t, "ATYPE"); atyS.toUpperCase();
            String achS = getTokValue(t, "ACH");
            String aopS = getTokValue(t, "AOP"); aopS.toUpperCase();
            String av1S = getTokValue(t, "AV1");
            String av2S = getTokValue(t, "AV2");
            String ahyS = getTokValue(t, "AHYS");
            String adbS = getTokValue(t, "ADBMS");
            String atolS = getTokValue(t, "ATOL");

            if (enS.length()) s.enabled = (enS.toInt() != 0);

            // modes
            if (modeS == "DI") s.mode = S_MODE_DI;
            else if (modeS == "RTC") s.mode = S_MODE_RTC;
            else if (modeS == "DI_RTC") s.mode = S_MODE_COMBINED;
            else if (modeS == "ANALOG") s.mode = S_MODE_ANALOG;
            else if (modeS == "ANALOG_DI") s.mode = S_MODE_ANALOG_DI;
            else if (modeS == "ANALOG_RTC") s.mode = S_MODE_ANALOG_RTC;
            else if (modeS == "ANALOG_DI_RTC") s.mode = S_MODE_ANALOG_DI_RTC;

            if (diS.length()) {
                int di = diS.toInt();
                if (di < 1) di = 1; if (di > 8) di = 8;
                s.triggerDI = (uint8_t)di;
            }

            if (edgeS.length()) {
                if (edgeS == "RISING") s.edge = EDGE_RISING;
                else if (edgeS == "FALLING") s.edge = EDGE_FALLING;
                else if (edgeS == "BOTH") s.edge = EDGE_BOTH;
                else if (edgeS == "HIGH") s.edge = EDGE_HIGH;
                else if (edgeS == "LOW") s.edge = EDGE_LOW;
                else s.edge = EDGE_BOTH;
            }

            if (startS.length()) { uint16_t m; if (parseHHMM(startS, m)) s.startMin = m; }
            if (endS.length()) { uint16_t m; if (parseHHMM(endS, m)) s.endMin = m; }

            if (recurS.length()) s.recurring = (recurS.toInt() != 0);

            if (daysS.length()) {
                int dm = daysS.toInt();
                if (dm < 0) dm = 0;
                if (dm > 127) dm = 127;
                s.daysMask = (uint8_t)dm;
            }

            if (rmaskS.length()) {
                int rm = rmaskS.toInt();
                if (rm < 0) rm = 0;
                if (rm > 63) rm = 63;
                s.relayMask = (uint8_t)rm;
            }

            if (ractS.length()) {
                if (ractS == "ON") s.relayAct = RELACT_ON;
                else if (ractS == "OFF") s.relayAct = RELACT_OFF;
                else s.relayAct = RELACT_TOGGLE;
            }

            if (d1mvS.length()) s.dac1mV = d1mvS.toInt();
            if (d2mvS.length()) s.dac2mV = d2mvS.toInt();

            if (d1rS.length()) { int r = d1rS.toInt(); s.dac1Range = (uint8_t)((r == 10) ? 10 : 5); }
            if (d2rS.length()) { int r = d2rS.toInt(); s.dac2Range = (uint8_t)((r == 10) ? 10 : 5); }

            if (buzenS.length()) s.buzEnable = (buzenS.toInt() != 0);
            if (buzfS.length()) { int v = buzfS.toInt(); if (v <= 0) v = 2000; s.buzFreq = (uint16_t)v; }
            if (buzonS.length()) { int v = buzonS.toInt(); if (v <= 0) v = 200;  s.buzOnMs = (uint16_t)v; }
            if (buzoffS.length()) { int v = buzoffS.toInt(); if (v < 0) v = 0;  s.buzOffMs = (uint16_t)v; }
            if (buzrepS.length()) { int v = buzrepS.toInt(); if (v <= 0) v = 1; if (v > 255) v = 255; s.buzRepeats = (uint8_t)v; }

            // Analog
            if (aenS.length()) s.a.enabled = (aenS.toInt() != 0);
            if (atyS.length()) s.a.type = (atyS == "CURR" || atyS == "CURRENT") ? A_TYPE_CURR : A_TYPE_VOLT;
            if (achS.length()) {
                int c = achS.toInt();
                if (c < 1) c = 1;
                if (c > 2) c = 2;
                s.a.ch = (uint8_t)c;
            }
            if (aopS.length()) {
                if (aopS == "ABOVE") s.a.op = A_OP_ABOVE;
                else if (aopS == "BELOW") s.a.op = A_OP_BELOW;
                else if (aopS == "EQUAL") s.a.op = A_OP_EQUAL;
                else s.a.op = A_OP_IN_RANGE;
            }
            if (av1S.length()) s.a.v1 = av1S.toInt();
            if (av2S.length()) s.a.v2 = av2S.toInt();
            if (ahyS.length()) s.a.hysteresis = ahyS.toInt();
            if (adbS.length()) {
                long db = adbS.toInt();
                if (db < 10) db = 10;
                if (db > 60000) db = 60000;
                s.a.debounceMs = (uint16_t)db;
            }
            if (atolS.length()) s.a.tol = atolS.toInt();

            // reset runtime flags
            s.firedForWindow = false;
            s.lastDiEdgeMs = 0;
            s.a.condition = false;
            s.a.pending = false;
            s.a.pendingSinceMs = 0;
        };

        if (sop == "LIST") {
            tcpSend("SCHED LIST BEGIN");
            for (uint8_t i = 0; i < g_schedCount; i++) {
                const ScheduleConfig& s = g_scheds[i];
                uint32_t id = i + 1;

                String modeTxt =
                    (s.mode == S_MODE_DI) ? "DI" :
                    (s.mode == S_MODE_RTC) ? "RTC" :
                    (s.mode == S_MODE_COMBINED) ? "DI_RTC" :
                    (s.mode == S_MODE_ANALOG) ? "ANALOG" :
                    (s.mode == S_MODE_ANALOG_DI) ? "ANALOG_DI" :
                    (s.mode == S_MODE_ANALOG_RTC) ? "ANALOG_RTC" :
                    "ANALOG_DI_RTC";

                String l = "SCHED ITEM ";
                l += "ID=" + String(id);
                l += " EN=" + String(s.enabled ? 1 : 0);
                l += " MODE=" + modeTxt;
                l += " DAYS=" + String(s.daysMask);
                l += " START=" + fmtHHMM(s.startMin);
                l += " END=" + fmtHHMM(s.endMin);
                l += " RECUR=" + String(s.recurring ? 1 : 0);
                l += " DI=" + String(s.triggerDI);

                String edgeTxt =
                    (s.edge == EDGE_RISING) ? "RISING" :
                    (s.edge == EDGE_FALLING) ? "FALLING" :
                    (s.edge == EDGE_BOTH) ? "BOTH" :
                    (s.edge == EDGE_HIGH) ? "HIGH" :
                    "LOW";

                l += " EDGE=" + edgeTxt;
                l += " RELMASK=" + String(s.relayMask);
                l += " RELACT=" + String(s.relayAct == RELACT_ON ? "ON" : (s.relayAct == RELACT_OFF ? "OFF" : "TOGGLE"));
                l += " DAC1MV=" + String(s.dac1mV);
                l += " DAC2MV=" + String(s.dac2mV);
                l += " DAC1R=" + String(s.dac1Range);
                l += " DAC2R=" + String(s.dac2Range);
                l += " BUZEN=" + String(s.buzEnable ? 1 : 0);
                l += " BUZFREQ=" + String(s.buzFreq);
                l += " BUZON=" + String(s.buzOnMs);
                l += " BUZOFF=" + String(s.buzOffMs);
                l += " BUZREP=" + String(s.buzRepeats);

                // analog extensions
                l += " AEN=" + String(s.a.enabled ? 1 : 0);
                l += " ATYPE=" + String(s.a.type == A_TYPE_CURR ? "CURR" : "VOLT");
                l += " ACH=" + String(s.a.ch);
                l += " AOP=" + String(
                    (s.a.op == A_OP_ABOVE) ? "ABOVE" :
                    (s.a.op == A_OP_BELOW) ? "BELOW" :
                    (s.a.op == A_OP_EQUAL) ? "EQUAL" : "IN_RANGE"
                );
                l += " AV1=" + String(s.a.v1);
                l += " AV2=" + String(s.a.v2);
                l += " AHYS=" + String(s.a.hysteresis);
                l += " ADBMS=" + String(s.a.debounceMs);
                l += " ATOL=" + String(s.a.tol);

                tcpSend(l);
            }
            tcpSend("SCHED LIST END");
            return;
        }

        if (sop == "CLEAR") {
            schedResetAll();
            tcpSend("SCHED OK CLEAR");
            return;
        }

        if (sop == "DEL") {
            int id = getTokValue(sargs, "ID").toInt();
            if (id <= 0 || id > g_schedCount) { tcpSend("ERR SCHED DEL"); return; }
            uint8_t idx = (uint8_t)(id - 1);
            schedDelete(idx);
            tcpSend(String("SCHED OK DEL ID=") + String(id));
            return;
        }

        if (sop == "DISABLE") {
            int id = getTokValue(sargs, "ID").toInt();
            if (id <= 0 || id > g_schedCount) { tcpSend("ERR SCHED DISABLE"); return; }
            uint8_t idx = (uint8_t)(id - 1);
            g_scheds[idx].enabled = false;
            schedSaveOne(idx);
            tcpSend(String("SCHED OK DISABLE ID=") + String(id));
            return;
        }

        if (sop == "UPSERT") {
            int id = getTokValue(sargs, "ID").toInt();
            if (id < 0) id = 0;

            if (id == 0) {
                if (g_schedCount >= SCHED_MAX) { tcpSend("ERR SCHED FULL"); return; }
                uint8_t idx = g_schedCount++;
                g_scheds[idx] = ScheduleConfig();
                parseInto(g_scheds[idx], sargs);
                schedSaveMeta();
                schedSaveOne(idx);
                tcpSend(String("SCHED OK UPSERT ID=") + String(idx + 1));
                return;
            }

            if (id > SCHED_MAX) { tcpSend("ERR SCHED ID"); return; }
            uint8_t idx = (uint8_t)(id - 1);

            if (idx >= g_schedCount) {
                while (g_schedCount <= idx && g_schedCount < SCHED_MAX) {
                    g_scheds[g_schedCount] = ScheduleConfig();
                    g_scheds[g_schedCount].enabled = false;
                    g_schedCount++;
                }
            }

            parseInto(g_scheds[idx], sargs);
            schedSaveMeta();
            schedSaveOne(idx);
            tcpSend(String("SCHED OK UPSERT ID=") + String(id));
            return;
        }

        tcpSend("ERR SCHED");
        return;
    }

    tcpSend("ERR UNKNOWN");
}

// ======================================================================
// SNAPSHOT ENCODING (extended with raw values + AI range)
// ======================================================================
void sendSnapshot(EthernetClient& c, bool json) {
    g_di.updateAllInputs();
    uint8_t diBits = g_di.getInputState();

    uint16_t vraw1 = g_ai.readRawVoltageChannel(0);
    uint16_t vraw2 = g_ai.readRawVoltageChannel(1);
    uint16_t iraw1 = g_ai.readRawCurrentChannel(0);
    uint16_t iraw2 = g_ai.readRawCurrentChannel(1);

    float v1 = g_ai.readVoltage(0);
    float v2 = g_ai.readVoltage(1);
    float i1 = g_ai.readCurrent(0);
    float i2 = g_ai.readCurrent(1);

    uint8_t relBits = g_relays.getAllStates();
    float dv0 = g_dac.isInitialized() ? g_dac.getVoltage(0) : 0;
    float dv1 = g_dac.isInitialized() ? g_dac.getVoltage(1) : 0;

    g_dht.update();
    float t1 = g_dht.getTemperature(0);
    float h1 = g_dht.getHumidity(0);
    float t2 = g_dht.getTemperature(1);
    float h2 = g_dht.getHumidity(1);
    uint8_t dsCnt = g_dht.getDS18B20Count();
    float ds0 = dsCnt > 0 ? g_dht.getDS18B20Temperature(0) : NAN;

    DateTime now = g_rtc.getDateTime(false);

    if (json) {
        String j = "{";
        j += "\"di\":" + String(diBits);
        j += ",\"rel\":" + String(relBits);

        // AI range
        j += ",\"ai_vrange\":" + String((int)g_ai.getVoltageRange());

        // raw arrays
        j += ",\"ai_v_raw\":[" + String(vraw1) + "," + String(vraw2) + "]";
        j += ",\"ai_i_raw\":[" + String(iraw1) + "," + String(iraw2) + "]";

        // scaled arrays
        j += ",\"ai_v\":[" + String(v1, 3) + "," + String(v2, 3) + "]";
        j += ",\"ai_i\":[" + String(i1, 3) + "," + String(i2, 3) + "]";

        j += ",\"dac_mv\":[" + String((int)lroundf(dv0 * 1000.0f)) + "," + String((int)lroundf(dv1 * 1000.0f)) + "]";

        j += ",\"dht\":[{";
        j += (isnan(t1) ? "\"t\":null" : "\"t\":" + String(t1, 2));
        j += ",";
        j += (isnan(h1) ? "\"h\":null" : "\"h\":" + String(h1, 2));
        j += "},{";
        j += (isnan(t2) ? "\"t\":null" : "\"t\":" + String(t2, 2));
        j += ",";
        j += (isnan(h2) ? "\"h\":null" : "\"h\":" + String(h2, 2));
        j += "}]";

        j += ",\"ds18_cnt\":" + String(dsCnt);
        if (!isnan(ds0)) j += ",\"ds18_0\":" + String(ds0, 2);

        j += ",\"rtc\":\"";
        char buf[24];
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
            now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
        j += buf;
        j += "\"}";
        c.println(j);
    }
    else {
        c.println(F("SNAPSHOT BEGIN"));
        c.print(F("DI_BITS=")); c.println(diBits, BIN);
        c.print(F("REL_BITS=")); c.println(relBits, BIN);
        c.print(F("AI_VRAW1=")); c.println(vraw1);
        c.print(F("AI_VRAW2=")); c.println(vraw2);
        c.print(F("AI_IRAW1=")); c.println(iraw1);
        c.print(F("AI_IRAW2=")); c.println(iraw2);
        c.print(F("AI_V1=")); c.println(v1, 3);
        c.print(F("AI_V2=")); c.println(v2, 3);
        c.print(F("AI_I1=")); c.println(i1, 3);
        c.print(F("AI_I2=")); c.println(i2, 3);
        c.print(F("DAC_V0=")); c.println(dv0, 3);
        c.print(F("DAC_V1=")); c.println(dv1, 3);
        c.println(F("END"));
    }
}

// ======================================================================
// SERIAL CONSOLE
// ======================================================================
void processSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r' || c == '\n') { if (g_serialCmd.length()) g_serialCmdReady = true; }
        else if (c == 0x08 || c == 0x7F) { if (g_serialCmd.length()) g_serialCmd.remove(g_serialCmd.length() - 1); }
        else { if (g_serialCmd.length() < 200) g_serialCmd += c; }
    }

    if (g_serialCmdReady) {
        String cmd = g_serialCmd;
        cmd.trim();
        if (cmd.length()) {
            Serial.print(F("> "));
            Serial.println(cmd);
            execSerialCommand(cmd);
        }
        g_serialCmd = "";
        g_serialCmdReady = false;
    }
}

void execSerialCommand(const String& raw) {
    String line = raw; line.trim();
    if (!line.length()) return;

    String upper = line; upper.toUpperCase();
    if (upper == "HELP" || upper == "?") { printHelp(); return; }
    if (upper == "STATUS") { printStatus(); return; }
    if (upper == "SERIALNET" || upper == "NETDUMP") { netcfgSerialDump(); return; }
    if (upper == "BOARDINFO") { hwInfoPrintBoardInfoSerial(); return; }

    handleTcpCommand(line);
}

void printHelp() {
    Serial.println(F("\n=== Serial Commands ==="));
    Serial.println(F("HELP / ?"));
    Serial.println(F("STATUS"));
    Serial.println(F("SERIALNET (print full network settings table)"));
    Serial.println(F("PING / SNAPJSON / REL ... (same as TCP)"));
    Serial.println(F("SCHED LIST / SCHED UPSERT ... / SCHED DEL ... / SCHED CLEAR"));
    Serial.println(F("NETCFG GET"));
    Serial.println(F("NETCFG SET DHCP=.. MAC=.. IP=.. MASK=.. GW=.. DNS=.. TCPPORT=.. APSSID=.. APPASS=.. AI_VRANGE=5|10 REBOOT=1"));
}

void printStatus() {
    Serial.println(F("\n=== STATUS ==="));
    Serial.println(F("-- Ethernet --"));
    Serial.print(F("Ready: ")); Serial.println(g_eth.isReady() ? "YES" : "NO");
    Serial.print(F("IP   : ")); Serial.println(g_eth.getIP());
    Serial.print(F("Link : ")); Serial.println(g_eth.getLinkStatus() ? "UP" : "DOWN");
    Serial.print(F("TCP Port: ")); Serial.println(g_tcpPort);

    Serial.println(F("-- Analog --"));
    Serial.print(F("AI Voltage Range: ")); Serial.print((int)g_ai.getVoltageRange()); Serial.println(F("V"));

    Serial.println(F("-- Scheduler --"));
    Serial.print(F("Count: ")); Serial.println(g_schedCount);

    for (uint8_t i = 0; i < g_schedCount; i++) {
        Serial.print(F("  #")); Serial.print(i + 1);
        Serial.print(F(" EN=")); Serial.print(g_scheds[i].enabled ? "1" : "0");
        Serial.print(F(" MODE=")); Serial.print((int)g_scheds[i].mode);
        Serial.print(F(" DAYS=")); Serial.print(g_scheds[i].daysMask, BIN);
        Serial.print(F(" WIN=")); Serial.print(fmtHHMM(g_scheds[i].startMin));
        Serial.print(F("-")); Serial.println(fmtHHMM(g_scheds[i].endMin));
    }
    Serial.println(F("=== END STATUS ===\n"));
}

// ======================================================================
// Scheduler time helpers
// ======================================================================
static bool parseHHMM(const String& s, uint16_t& outMin) {
    int colon = s.indexOf(':');
    if (colon < 0) return false;
    int hh = s.substring(0, colon).toInt();
    int mm = s.substring(colon + 1).toInt();
    if (hh < 0 || hh > 23) return false;
    if (mm < 0 || mm > 59) return false;
    outMin = (uint16_t)(hh * 60 + mm);
    return true;
}

static String fmtHHMM(uint16_t mins) {
    uint8_t hh = mins / 60;
    uint8_t mm = mins % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02u:%02u", hh, mm);
    return String(buf);
}

static uint16_t rtcMinutesNow() {
    DateTime now = g_rtc.getDateTime(false);
    return (uint16_t)(now.hour() * 60 + now.minute());
}

// RTClib dow: 0=Sun..6=Sat. VB mask: bit0=Mon..bit6=Sun.
static uint8_t rtcDaysMaskNowMonBit0() {
    DateTime now = g_rtc.getDateTime(false);
    uint8_t dow = now.dayOfTheWeek();
    switch (dow) {
    case 1: return (1 << 0);
    case 2: return (1 << 1);
    case 3: return (1 << 2);
    case 4: return (1 << 3);
    case 5: return (1 << 4);
    case 6: return (1 << 5);
    case 0: default: return (1 << 6);
    }
}

static bool isWithinWindow(uint16_t nowMin, uint16_t startMin, uint16_t endMin) {
    if (startMin == endMin) return true;
    if (startMin < endMin) return (nowMin >= startMin && nowMin < endMin);
    return (nowMin >= startMin || nowMin < endMin);
}

// ======================================================================
// Scheduler Preferences load/save
// ======================================================================
static void schedSaveMeta() {
    g_prefs.begin(PREF_NS_SCHED_META, false);
    g_prefs.putUChar(KEY_SCHED_COUNT, g_schedCount);
    g_prefs.end();
}

static void schedSaveOne(uint8_t idx) {
    if (idx >= SCHED_MAX) return;
    String ns = schedNs(idx);
    g_prefs.begin(ns.c_str(), false);

    const ScheduleConfig& s = g_scheds[idx];
    g_prefs.putBool(KEY_S_EN, s.enabled);
    g_prefs.putUChar(KEY_S_MODE, (uint8_t)s.mode);
    g_prefs.putUChar(KEY_S_DI, s.triggerDI);
    g_prefs.putUChar(KEY_S_EDGE, (uint8_t)s.edge);
    g_prefs.putUShort(KEY_S_ST, s.startMin);
    g_prefs.putUShort(KEY_S_ENM, s.endMin);
    g_prefs.putBool(KEY_S_RECUR, s.recurring);
    g_prefs.putUChar(KEY_S_DAYS, s.daysMask);

    g_prefs.putUChar(KEY_S_RMASK, s.relayMask);
    g_prefs.putUChar(KEY_S_RACT, (uint8_t)s.relayAct);

    g_prefs.putInt(KEY_S_D1MV, s.dac1mV);
    g_prefs.putInt(KEY_S_D2MV, s.dac2mV);
    g_prefs.putUChar(KEY_S_D1R, s.dac1Range);
    g_prefs.putUChar(KEY_S_D2R, s.dac2Range);

    g_prefs.putBool(KEY_S_BUZEN, s.buzEnable);
    g_prefs.putUShort(KEY_S_BUZF, s.buzFreq);
    g_prefs.putUShort(KEY_S_BUZON, s.buzOnMs);
    g_prefs.putUShort(KEY_S_BUZOFF, s.buzOffMs);
    g_prefs.putUChar(KEY_S_BUZREP, s.buzRepeats);

    // NEW analog trigger
    g_prefs.putBool(KEY_A_EN, s.a.enabled);
    g_prefs.putUChar(KEY_A_TYPE, (uint8_t)s.a.type);
    g_prefs.putUChar(KEY_A_CH, s.a.ch);
    g_prefs.putUChar(KEY_A_OP, (uint8_t)s.a.op);
    g_prefs.putInt(KEY_A_V1, (int)s.a.v1);
    g_prefs.putInt(KEY_A_V2, (int)s.a.v2);
    g_prefs.putInt(KEY_A_HYS, (int)s.a.hysteresis);
    g_prefs.putUShort(KEY_A_DBMS, s.a.debounceMs);
    g_prefs.putInt(KEY_A_TOL, (int)s.a.tol);

    g_prefs.end();
}

static void schedDelete(uint8_t idx) {
    if (idx >= g_schedCount) return;

    for (uint8_t i = idx; i + 1 < g_schedCount; i++) {
        g_scheds[i] = g_scheds[i + 1];
        schedSaveOne(i);
    }

    uint8_t last = g_schedCount - 1;
    String ns = schedNs(last);
    g_prefs.begin(ns.c_str(), false);
    g_prefs.clear();
    g_prefs.end();

    g_schedCount--;
    schedSaveMeta();
}

static void schedResetAll() {
    for (uint8_t i = 0; i < SCHED_MAX; i++) {
        String ns = schedNs(i);
        g_prefs.begin(ns.c_str(), false);
        g_prefs.clear();
        g_prefs.end();
        g_scheds[i] = ScheduleConfig();
        g_scheds[i].enabled = false;
    }
    g_schedCount = 0;
    schedSaveMeta();
}

static void schedLoadAll() {
    g_schedCount = 0;

    g_prefs.begin(PREF_NS_SCHED_META, true);
    uint8_t count = g_prefs.getUChar(KEY_SCHED_COUNT, 0);
    g_prefs.end();

    if (count > SCHED_MAX) count = SCHED_MAX;
    g_schedCount = count;

    for (uint8_t i = 0; i < SCHED_MAX; i++) {
        g_scheds[i] = ScheduleConfig();
        g_scheds[i].enabled = false;
    }

    for (uint8_t i = 0; i < g_schedCount; i++) {
        String ns = schedNs(i);
        g_prefs.begin(ns.c_str(), true);

        ScheduleConfig s;
        s.enabled = g_prefs.getBool(KEY_S_EN, false);
        s.mode = (SchedMode)g_prefs.getUChar(KEY_S_MODE, (uint8_t)S_MODE_DI);
        s.triggerDI = g_prefs.getUChar(KEY_S_DI, 1);
        s.edge = (EdgeMode)g_prefs.getUChar(KEY_S_EDGE, (uint8_t)EDGE_RISING);
        // validate edge
        if ((uint8_t)s.edge > (uint8_t)EDGE_LOW) s.edge = EDGE_RISING;
        s.startMin = g_prefs.getUShort(KEY_S_ST, 8 * 60);
        s.endMin = g_prefs.getUShort(KEY_S_ENM, 17 * 60);
        s.recurring = g_prefs.getBool(KEY_S_RECUR, true);
        s.daysMask = g_prefs.getUChar(KEY_S_DAYS, 0x7F);

        s.relayMask = g_prefs.getUChar(KEY_S_RMASK, 0);
        s.relayAct = (RelayAct)g_prefs.getUChar(KEY_S_RACT, (uint8_t)RELACT_TOGGLE);

        s.dac1mV = g_prefs.getInt(KEY_S_D1MV, 0);
        s.dac2mV = g_prefs.getInt(KEY_S_D2MV, 0);
        s.dac1Range = g_prefs.getUChar(KEY_S_D1R, 5);
        s.dac2Range = g_prefs.getUChar(KEY_S_D2R, 5);

        s.buzEnable = g_prefs.getBool(KEY_S_BUZEN, false);
        s.buzFreq = g_prefs.getUShort(KEY_S_BUZF, 2000);
        s.buzOnMs = g_prefs.getUShort(KEY_S_BUZON, 200);
        s.buzOffMs = g_prefs.getUShort(KEY_S_BUZOFF, 200);
        s.buzRepeats = g_prefs.getUChar(KEY_S_BUZREP, 5);

        // NEW analog load (defaults safe if missing)
        s.a.enabled = g_prefs.getBool(KEY_A_EN, false);
        s.a.type = (AnalogType)g_prefs.getUChar(KEY_A_TYPE, (uint8_t)A_TYPE_VOLT);
        s.a.ch = g_prefs.getUChar(KEY_A_CH, 1);
        s.a.op = (AnalogOp)g_prefs.getUChar(KEY_A_OP, (uint8_t)A_OP_ABOVE);
        s.a.v1 = g_prefs.getInt(KEY_A_V1, 0);
        s.a.v2 = g_prefs.getInt(KEY_A_V2, 0);
        s.a.hysteresis = g_prefs.getInt(KEY_A_HYS, 50);
        s.a.debounceMs = g_prefs.getUShort(KEY_A_DBMS, 200);
        s.a.tol = g_prefs.getInt(KEY_A_TOL, 50);

        g_prefs.end();

        if (s.triggerDI < 1) s.triggerDI = 1;
        if (s.triggerDI > 8) s.triggerDI = 8;
        if (s.daysMask > 127) s.daysMask = 127;
        if (s.relayMask > 63) s.relayMask = 63;
        if (s.dac1Range != 10) s.dac1Range = 5;
        if (s.dac2Range != 10) s.dac2Range = 5;

        if (s.a.ch < 1) s.a.ch = 1;
        if (s.a.ch > 2) s.a.ch = 2;
        if (s.a.debounceMs < 10) s.a.debounceMs = 10;
        if (s.a.hysteresis < 0) s.a.hysteresis = 0;
        if (s.a.tol < 0) s.a.tol = 0;

        g_scheds[i] = s;
    }
}

// ======================================================================
// Scheduler output apply
// ======================================================================
static void schedApplyOutputs(const ScheduleConfig& s) {
    if (g_relays.isConnected() && s.relayMask) {
        for (uint8_t r = 1; r <= NUM_RELAY_OUTPUTS; r++) {
            uint8_t bit = (1 << (r - 1));
            if ((s.relayMask & bit) == 0) continue;

            switch (s.relayAct) {
            case RELACT_ON:     g_relays.turnOn(r); break;
            case RELACT_OFF:    g_relays.turnOff(r); break;
            case RELACT_TOGGLE: g_relays.toggle(r); break;
            }
        }
    }

    if (g_dac.isInitialized()) {
        applyScheduleDacRange(s);
        g_dac.setVoltage(0, (float)s.dac1mV / 1000.0f);
        g_dac.setVoltage(1, (float)s.dac2mV / 1000.0f);
    }

    if (s.buzEnable) {
        buzzStart(s.buzFreq, s.buzOnMs, s.buzOffMs, s.buzRepeats);
    }
}

// ======================================================================
// Scheduler main update (OR logic for mixed modes)
// Fires once per window concept for RTC window; for analog and DI, we fire on event.
// ======================================================================
static void schedUpdate() {
    g_di.updateAllInputs();
    uint8_t diNow = g_di.getInputState();
    uint8_t diChanged = (uint8_t)(diNow ^ g_lastDiBits);

    uint16_t nowMin = rtcMinutesNow();
    uint8_t  dowMask = rtcDaysMaskNowMonBit0();

    for (uint8_t i = 0; i < g_schedCount; i++) {
        ScheduleConfig& s = g_scheds[i];
        if (!s.enabled) continue;

        // day filter applies for RTC window and also for analog modes (as per existing design)
        if ((s.daysMask & dowMask) == 0) {
            s.firedForWindow = false;
            continue;
        }

        bool inWindow = isWithinWindow(nowMin, s.startMin, s.endMin);

        // update analog stable state (debounced)
        bool analogChanged = false;
        if (s.a.enabled) analogChanged = analogStableConditionUpdate(s.a);
        bool analogTrue = s.a.enabled ? s.a.condition : false;

        // DI trigger evaluation: edge-based (RISING/FALLING/BOTH) or level-based (HIGH/LOW)
        bool diEvent = false;
        bool diLevelTrue = false;

        bool needsDI =
            (s.mode == S_MODE_DI || s.mode == S_MODE_COMBINED ||
                s.mode == S_MODE_ANALOG_DI || s.mode == S_MODE_ANALOG_DI_RTC);

        if (needsDI) {
            uint8_t diIdx0 = (uint8_t)(s.triggerDI - 1);
            uint8_t mask = (1 << diIdx0);

            bool curState = (diNow & mask) != 0;
            bool prevState = (g_lastDiBits & mask) != 0;

            bool rising = (!prevState && curState);
            bool falling = (prevState && !curState);

            // Level meaning
            if (s.edge == EDGE_HIGH) diLevelTrue = curState;
            else if (s.edge == EDGE_LOW) diLevelTrue = !curState;

            // Edge event meaning
            if (s.edge == EDGE_RISING || s.edge == EDGE_FALLING || s.edge == EDGE_BOTH) {
                bool edgeOk = false;
                if (s.edge == EDGE_BOTH) edgeOk = (rising || falling);
                else if (s.edge == EDGE_RISING) edgeOk = rising;
                else if (s.edge == EDGE_FALLING) edgeOk = falling;

                if (edgeOk) {
                    uint32_t nowMs = millis();
                    if (nowMs - s.lastDiEdgeMs >= 200) { // debounce edges
                        s.lastDiEdgeMs = nowMs;
                        diEvent = true;
                    }
                }
            }
        }

        // RTC-only behavior: once-per-window
        if (s.mode == S_MODE_RTC) {
            if (inWindow) {
                if (!s.firedForWindow) {
                    s.firedForWindow = true;
                    schedApplyOutputs(s);
                }
            }
            else {
                s.firedForWindow = false;
            }
            continue;
        }

        // DI+RTC combined (existing): DI edge must occur AND be inside window
        if (s.mode == S_MODE_COMBINED) {
            // For edge modes: fire when the edge occurs inside window (same as before).
            // For level modes (HIGH/LOW): fire once per time window when the level is true.
            if (s.edge == EDGE_HIGH || s.edge == EDGE_LOW) {
                if (inWindow) {
                    if (diLevelTrue && !s.firedForWindow) {
                        s.firedForWindow = true;
                        schedApplyOutputs(s);
                    }
                }
                else {
                    s.firedForWindow = false;
                }
            }
            else {
                if (diEvent && inWindow) {
                    schedApplyOutputs(s);
                }
            }
            continue;
        }

        // DI-only (existing): fire on DI edge
        if (s.mode == S_MODE_DI) {
            // Edge modes: fire on edge.
            // Level modes: fire once when level becomes true (debounced by firedForWindow as a latch until it goes false).
            if (s.edge == EDGE_HIGH || s.edge == EDGE_LOW) {
                if (diLevelTrue) {
                    if (!s.firedForWindow) {
                        s.firedForWindow = true;
                        schedApplyOutputs(s);
                    }
                }
                else {
                    // allow next time level becomes true
                    s.firedForWindow = false;
                }
            }
            else {
                if (diEvent) schedApplyOutputs(s);
            }
            continue;
        }

        // NEW: Analog-only: treat analog "true" entry as event.
        // We fire when condition becomes true (debounced) AND we are in allowed time window?:
        // You requested "once per window" behavior. For Analog-only, we interpret this as:
        // - if time window is used, then fire first time analog becomes true while inWindow.
        // - if start==end (full day), then effectively fire on each true transition (debounced).
        if (s.mode == S_MODE_ANALOG) {
            if (!inWindow) {
                // if outside window, reset firedForWindow so it can fire next time inside
                s.firedForWindow = false;
                continue;
            }
            if (analogChanged && analogTrue) {
                if (!s.firedForWindow) {
                    s.firedForWindow = true;
                    schedApplyOutputs(s);
                }
            }
            // If analog condition goes false, allow future fire in same window? (strict once-per-window says no)
            // Keep it strict: do not clear firedForWindow until window ends.
            continue;
        }

        // NEW mixed OR logic:
        // ANALOG_DI: fire if (diEvent OR analog true event) but once per window gating applies based on time window.
        // You requested "once per window", so:
        // - only allow one fire while inWindow
        // - reset when leaving window
        //
        // For *_DI_*: diEvent is instantaneous.
        // For analog: use (analogChanged && analogTrue) as the event.
        bool analogEvent = (analogChanged && analogTrue);
        bool rtcEvent = false;

        if (s.mode == S_MODE_ANALOG_DI || s.mode == S_MODE_ANALOG_RTC || s.mode == S_MODE_ANALOG_DI_RTC) {
            if (!inWindow) {
                s.firedForWindow = false;
                continue;
            }

            if (s.mode == S_MODE_ANALOG_RTC || s.mode == S_MODE_ANALOG_DI_RTC) {
                // RTC window itself counts as an event, but only once per window (same as old RTC mode)
                rtcEvent = !s.firedForWindow; // allow rtcEvent to cause one fire, but it will set firedForWindow
            }

            // DI event OR DI level true depending on edge mode
            bool diTrig = false;
            if (needsDI) {
                if (s.edge == EDGE_HIGH || s.edge == EDGE_LOW) diTrig = diLevelTrue;
                else diTrig = diEvent;
            }

            bool any = false;
            if (s.mode == S_MODE_ANALOG_DI) any = (diTrig || analogEvent);
            else if (s.mode == S_MODE_ANALOG_RTC) any = (analogEvent || rtcEvent);
            else any = (diTrig || analogEvent || rtcEvent);

            if (any && !s.firedForWindow) {
                s.firedForWindow = true;
                schedApplyOutputs(s);
            }
            continue;
        }
    }

    g_lastDiBits = diNow;
}

// ======================================================================
// Buzzer pattern runtime
// ======================================================================
static void buzzStart(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t repeats) {
    if (repeats == 0) repeats = 1;
    g_buzz.active = true;
    g_buzz.phaseOn = true;
    g_buzz.freq = freq ? freq : 2000;
    g_buzz.onMs = onMs ? onMs : 200;
    g_buzz.offMs = offMs;
    g_buzz.repeats = repeats;
    g_buzz.step = 0;
    g_buzz.phaseStartMs = millis();
    g_rtc.simpleBeep(g_buzz.freq, g_buzz.onMs);
}

static void buzzStop() {
    g_buzz.active = false;
    g_buzz.phaseOn = false;
    g_buzz.step = 0;
    g_rtc.stopBeep();
}

static void buzzUpdate() {
    if (!g_buzz.active) return;

    uint32_t now = millis();
    if (g_buzz.phaseOn) {
        if (now - g_buzz.phaseStartMs >= g_buzz.onMs) {
            g_rtc.stopBeep();
            g_buzz.phaseOn = false;
            g_buzz.phaseStartMs = now;
            g_buzz.step++;
            if (g_buzz.step >= g_buzz.repeats) {
                g_buzz.active = false;
            }
        }
    }
    else {
        if (now - g_buzz.phaseStartMs >= g_buzz.offMs) {
            if (!g_buzz.active) return;
            g_buzz.phaseOn = true;
            g_buzz.phaseStartMs = now;
            g_rtc.simpleBeep(g_buzz.freq, g_buzz.onMs);
        }
    }
}

// ======================================================================
// Board Info (unchanged)
// ======================================================================
static void hwInfoEnsureDefaults() {
    g_prefs.begin(PREF_NS_HWINFO, false);

    if (!g_prefs.isKey(KEY_BOARD_NAME)) g_prefs.putString(KEY_BOARD_NAME, "KC_Link A8R-M");
    if (!g_prefs.isKey(KEY_BOARD_SERIAL)) {
        String sn = String("A8RM-G4V0F215PFA8-") + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
        sn.toUpperCase();
        g_prefs.putString(KEY_BOARD_SERIAL, sn);
    }
    if (!g_prefs.isKey(KEY_CREATED)) g_prefs.putString(KEY_CREATED, String(__DATE__));
    if (!g_prefs.isKey(KEY_MANUFACTURER)) g_prefs.putString(KEY_MANUFACTURER, MANUFACTURER_STR);
    if (!g_prefs.isKey(KEY_FW_VER)) g_prefs.putString(KEY_FW_VER, FW_VERSION_STR);
    if (!g_prefs.isKey(KEY_HW_VER)) g_prefs.putString(KEY_HW_VER, HW_VERSION_STR);
    if (!g_prefs.isKey(KEY_YEAR)) {
        String d = String(__DATE__);
        String y = (d.length() >= 4) ? d.substring(d.length() - 4) : String("2025");
        g_prefs.putString(KEY_YEAR, y);
    }
    g_prefs.end();
}

static void hwInfoPrintBoardInfoSerial() {
    g_prefs.begin(PREF_NS_HWINFO, true);
    String board = g_prefs.getString(KEY_BOARD_NAME, "KC_Link A8R-M");
    String sn = g_prefs.getString(KEY_BOARD_SERIAL, "");
    String created = g_prefs.getString(KEY_CREATED, "");
    String mfg = g_prefs.getString(KEY_MANUFACTURER, MANUFACTURER_STR);
    String fw = g_prefs.getString(KEY_FW_VER, FW_VERSION_STR);
    String hw = g_prefs.getString(KEY_HW_VER, HW_VERSION_STR);
    String year = g_prefs.getString(KEY_YEAR, "");
    g_prefs.end();

    String mac = espMacToString();

    Serial.println("BOARDINFO BEGIN");
    Serial.println(String("BOARD=") + board);
    Serial.println(String("SERIAL=") + sn);
    Serial.println(String("CREATED=") + created);
    Serial.println(String("MANUFACTURER=") + mfg);
    Serial.println(String("MAC=") + mac);
    Serial.println(String("FW=") + fw);
    Serial.println(String("HW=") + hw);
    Serial.println(String("YEAR=") + year);
    Serial.println("BOARDINFO END");
}

static void hwInfoPrintBoardInfoTcp() {
    g_prefs.begin(PREF_NS_HWINFO, true);
    String board = g_prefs.getString(KEY_BOARD_NAME, "KC_Link A8R-M");
    String sn = g_prefs.getString(KEY_BOARD_SERIAL, "");
    String created = g_prefs.getString(KEY_CREATED, "");
    String mfg = g_prefs.getString(KEY_MANUFACTURER, MANUFACTURER_STR);
    String fw = g_prefs.getString(KEY_FW_VER, FW_VERSION_STR);
    String hw = g_prefs.getString(KEY_HW_VER, HW_VERSION_STR);
    String year = g_prefs.getString(KEY_YEAR, "");
    g_prefs.end();

    String mac = espMacToString();

    tcpSend("BOARDINFO BEGIN");
    tcpSend(String("BOARD=") + board);
    tcpSend(String("SERIAL=") + sn);
    tcpSend(String("CREATED=") + created);
    tcpSend(String("MANUFACTURER=") + mfg);
    tcpSend(String("MAC=") + mac);
    tcpSend(String("FW=") + fw);
    tcpSend(String("HW=") + hw);
    tcpSend(String("YEAR=") + year);
    tcpSend("BOARDINFO END");
}
