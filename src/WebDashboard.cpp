#include "WebDashboard.h"
#include <LittleFS.h>

static DashboardMqttInfo g_info;

static String contentTypeFromPath(const String& path) {
    if (path.endsWith(".html")) return "text/html; charset=utf-8";
    if (path.endsWith(".css"))  return "text/css; charset=utf-8";
    if (path.endsWith(".js"))   return "application/javascript; charset=utf-8";
    if (path.endsWith(".json")) return "application/json; charset=utf-8";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".svg"))  return "image/svg+xml";
    if (path.endsWith(".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

void WebDashboard::addNoCacheHeaders(WebServer& server) {
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
}

String WebDashboard::jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:   out += c; break;
        }
    }
    return out;
}

bool WebDashboard::begin(WebServer& server, const DashboardMqttInfo& info) {
    g_info = info;

    bool fsOk = LittleFS.begin(true);

    server.on("/api/mqttinfo", HTTP_GET, [&server]() {
        addNoCacheHeaders(server);

        String json = "{";
        json += "\"baseTopic\":\"" + jsonEscape(g_info.baseTopic) + "\",";
        json += "\"clientId\":\"" + jsonEscape(g_info.clientId) + "\",";
        json += "\"fullBase\":\"" + jsonEscape(g_info.fullBase) + "\",";
        json += "\"brokerWsHost\":\"" + jsonEscape(g_info.brokerWsHost) + "\",";
        json += "\"brokerWsPort\":" + String(g_info.brokerWsPort) + ",";
        json += "\"brokerWsPath\":\"" + jsonEscape(g_info.brokerWsPath.length() ? g_info.brokerWsPath : String("/")) + "\",";

        // NEW: WS auth fields (may be empty)
        json += "\"brokerWsUser\":\"" + jsonEscape(g_info.brokerWsUser) + "\",";
        json += "\"brokerWsPass\":\"" + jsonEscape(g_info.brokerWsPass) + "\"";
        json += "}";

        server.send(200, "application/json; charset=utf-8", json);
        });

    server.on("/", HTTP_GET, [&server]() {
        addNoCacheHeaders(server);
        if (!LittleFS.exists("/index.html")) {
            server.send(404, "text/plain; charset=utf-8", "index.html not found in LittleFS");
            return;
        }
        File f = LittleFS.open("/index.html", "r");
        server.streamFile(f, "text/html; charset=utf-8");
        f.close();
        });

    server.onNotFound([&server]() {
        addNoCacheHeaders(server);

        String path = server.uri();
        if (path == "/") path = "/index.html";

        if (path.indexOf("..") >= 0) {
            server.send(400, "text/plain; charset=utf-8", "Bad path");
            return;
        }

        if (!LittleFS.exists(path)) {
            server.send(404, "text/plain; charset=utf-8", "Not found: " + path);
            return;
        }

        File f = LittleFS.open(path, "r");
        server.streamFile(f, contentTypeFromPath(path));
        f.close();
        });

    return fsOk;
}