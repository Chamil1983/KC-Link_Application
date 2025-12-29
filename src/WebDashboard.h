#pragma once
#include <Arduino.h>
#include <WebServer.h>

struct DashboardMqttInfo {
	String baseTopic;
	String clientId;
	String fullBase;

	String brokerWsHost;
	uint16_t brokerWsPort = 9001;
	String brokerWsPath = "/";

	// NEW: Optional WS auth
	String brokerWsUser;
	String brokerWsPass;
};

class WebDashboard {
public:
	static bool begin(WebServer& server, const DashboardMqttInfo& info);

private:
	static void addNoCacheHeaders(WebServer& server);
	static String jsonEscape(const String& s);
};