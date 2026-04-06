#ifndef FIRST_WIFI_PROVISIONING_H
#define FIRST_WIFI_PROVISIONING_H

#include <Arduino.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

class FirstWifiProvisioning {
public:
    void begin(const char* apSsid, const char* apPass, const char* credentialPath = "/wifi.cfg");
    bool connectStored(uint32_t timeoutMs = 15000);
    void connectStoredAsync(uint32_t timeoutMs = 15000);
    void startPortal();
    void loop();

    bool isPortalActive() const;
    bool isConnected() const;
    bool isConnecting() const;
    String statusMessage() const;
    String ssid() const;
    IPAddress localIP() const;

private:
    WebServer server_{80};
    DNSServer dnsServer_;
    String apSsid_;
    String apPass_;
    String credentialPath_;
    String savedSsid_;
    String savedPass_;
    bool portalActive_ = false;
    bool connecting_ = false;
    uint32_t connectStartMs_ = 0;
    uint32_t connectTimeoutMs_ = 0;
    String statusMessage_ = "WiFi: non initialise";

    bool loadCredentials();
    bool saveCredentials(const String& ssid, const String& password);
    bool connectToSaved(uint32_t timeoutMs);
    void setupRoutes();
    String buildPortalPage() const;
    String buildResultPage(const String& title, const String& body) const;
    static String urlDecode(const String& value);
    static String htmlEscape(const String& value);
};

#endif
