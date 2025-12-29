static const char* MQTT_BROKER_IP = "192.168.4.2";
static const uint16_t MQTT_BROKER_PORT = 1883;
static const uint16_t MQTT_BROKER_WS_PORT = 9002; // set to your Mosquitto WebSockets listener port (commonly 9001)

// NEW: must match mosquitto password_file
static const char* MQTT_WS_USER = "webdash";
static const char* MQTT_WS_PASS = "1234";


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

// ===== NEW: WiFi STA (station) settings stored in prefs =====
static const char* KEY_STA_ENABLED = "sta_en";
static const char* KEY_STA_SSID = "sta_ssid";
static const char* KEY_STA_PASS = "sta_pass";
static const char* KEY_STA_DHCP = "sta_dhcp";
static const char* KEY_STA_IP = "sta_ip";
static const char* KEY_STA_MASK = "sta_mask";
static const char* KEY_STA_GW = "sta_gw";
static const char* KEY_STA_DNS = "sta_dns";


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