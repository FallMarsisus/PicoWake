#include "FirstWifiProvisioning.h"

#define WIFI_LOG(fmt, ...) Serial.printf("[WIFI] " fmt "\n", ##__VA_ARGS__)

void FirstWifiProvisioning::begin(const char* apSsid, const char* apPass, const char* credentialPath) {
    apSsid_ = apSsid ? apSsid : "PicoWake-Setup";
    apPass_ = apPass ? apPass : "picowake123";
    credentialPath_ = credentialPath ? credentialPath : "/wifi.cfg";
    portalActive_ = false;
    connecting_ = false;
    statusMessage_ = "WiFi: initialization...";
    WIFI_LOG("begin apSsid=%s credentialPath=%s", apSsid_.c_str(), credentialPath_.c_str());
    loadCredentials();
}

bool FirstWifiProvisioning::loadCredentials() {
    WIFI_LOG("loadCredentials path=%s", credentialPath_.c_str());
    savedSsid_ = String();
    savedPass_ = String();

    if (!LittleFS.exists(credentialPath_)) {
        statusMessage_ = "WiFi: aucune cred. sauvegardee";
        WIFI_LOG("credentials file not found");
        return false;
    }

    File file = LittleFS.open(credentialPath_, "r");
    if (!file) {
        statusMessage_ = "WiFi: lecture cred impossible";
        WIFI_LOG("failed to open credentials file");
        return false;
    }

    savedSsid_ = file.readStringUntil('\n');
    savedPass_ = file.readStringUntil('\n');
    savedSsid_.trim();
    savedPass_.trim();
    file.close();

    if (savedSsid_.isEmpty()) {
        statusMessage_ = "WiFi: cred vides";
        WIFI_LOG("credentials invalid: empty SSID");
        return false;
    }

    statusMessage_ = "WiFi: cred chargees";
    WIFI_LOG("credentials loaded ssid=%s", savedSsid_.c_str());
    return true;
}

bool FirstWifiProvisioning::saveCredentials(const String& ssid, const String& password) {
    WIFI_LOG("saveCredentials ssid=%s", ssid.c_str());
    File file = LittleFS.open(credentialPath_, "w");
    if (!file) {
        statusMessage_ = "WiFi: sauvegarde impossible";
        WIFI_LOG("save failed: cannot open file");
        return false;
    }

    file.println(ssid);
    file.println(password);
    file.close();

    savedSsid_ = ssid;
    savedPass_ = password;
    return true;
}

bool FirstWifiProvisioning::connectToSaved(uint32_t timeoutMs) {
    if (savedSsid_.isEmpty()) {
        statusMessage_ = "WiFi: aucun reseau memorise";
        WIFI_LOG("connectToSaved aborted: empty saved SSID");
        return false;
    }

    WIFI_LOG("connectToSaved start ssid=%s timeout=%lu", savedSsid_.c_str(), static_cast<unsigned long>(timeoutMs));
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(savedSsid_.c_str(), savedPass_.c_str());

    const uint32_t start = millis();
    while ((WiFi.status() != WL_CONNECTED) && (millis() - start < timeoutMs)) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        statusMessage_ = "WiFi: connecte a " + savedSsid_;
        WIFI_LOG("connectToSaved success ip=%s", WiFi.localIP().toString().c_str());
        return true;
    }

    statusMessage_ = "WiFi: echec connexion";
    WIFI_LOG("connectToSaved timeout/failure status=%d", static_cast<int>(WiFi.status()));
    return false;
}

bool FirstWifiProvisioning::connectStored(uint32_t timeoutMs) {
    WIFI_LOG("connectStored timeout=%lu", static_cast<unsigned long>(timeoutMs));
    if (!loadCredentials()) {
        return false;
    }
    return connectToSaved(timeoutMs);
}

void FirstWifiProvisioning::connectStoredAsync(uint32_t timeoutMs) {
    connecting_ = false;
    WIFI_LOG("connectStoredAsync timeout=%lu", static_cast<unsigned long>(timeoutMs));

    if (!loadCredentials()) {
        return;
    }

    if (savedSsid_.isEmpty()) {
        statusMessage_ = "WiFi: aucun reseau memorise";
        WIFI_LOG("connectStoredAsync aborted: no saved SSID");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(savedSsid_.c_str(), savedPass_.c_str());

    connectStartMs_ = millis();
    connectTimeoutMs_ = timeoutMs;
    connecting_ = true;
    portalActive_ = false;
    statusMessage_ = "WiFi: connexion en cours";
    WIFI_LOG("async connect started ssid=%s", savedSsid_.c_str());
}

void FirstWifiProvisioning::startPortal() {
    portalActive_ = true;
    WIFI_LOG("startPortal ssid=%s", apSsid_.c_str());
    
    // 1. On coupe tout proprement
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);

    // 2. ON CONFIGURE L'IP AVANT DE DÉMARRER L'AP
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

    // 3. ON DÉMARRE LE RÉSEAU AVEC LE MOT DE PASSE
    // Le téléphone demandera "picowake123" pour se connecter
    const bool started = WiFi.softAP(apSsid_.c_str(), apPass_.c_str());
    delay(100); // Laisse au hardware le temps de créer l'interface

    setupRoutes();
    server_.begin();
    dnsServer_.stop();
    
    // On force le DNS sur l'IP statique qu'on vient de définir
    dnsServer_.start(53, "*", IPAddress(192, 168, 4, 1)); 
    WIFI_LOG("portal DNS started on 192.168.4.1");
    
    if (started) {
        statusMessage_ = "WiFi: portail actif (" + apSsid_ + ")";
        WIFI_LOG("portal started ip=%s", WiFi.softAPIP().toString().c_str());
    } else {
        statusMessage_ = "WiFi: echec demarrage AP";
        WIFI_LOG("portal start failed");
    }
}

void FirstWifiProvisioning::setupRoutes() {
    server_.on("/", HTTP_GET, [this]() {
        WIFI_LOG("HTTP / from %s", server_.client().remoteIP().toString().c_str());
        server_.send(200, "text/html; charset=utf-8", buildPortalPage());
    });

    server_.on("/save", HTTP_GET, [this]() {
        WIFI_LOG("HTTP /save from %s", server_.client().remoteIP().toString().c_str());
        const String ssid = urlDecode(server_.arg("ssid"));
        const String password = urlDecode(server_.arg("pass"));

        if (ssid.isEmpty()) {
            WIFI_LOG("/save rejected: empty SSID");
            server_.send(400, "text/html; charset=utf-8", buildResultPage("Erreur", "SSID manquant."));
            return;
        }

        saveCredentials(ssid, password);
        server_.send(200, "text/html; charset=utf-8", buildResultPage("Sauvegarde OK", "Tentative de connexion au reseau..."));

        portalActive_ = false;
        dnsServer_.stop();
        server_.stop();
        WIFI_LOG("portal stopped, trying async connect with new creds");

        connectStoredAsync(15000);
    });

    server_.onNotFound([this]() {
        WIFI_LOG("HTTP notFound path=%s", server_.uri().c_str());
        server_.send(200, "text/html; charset=utf-8", buildPortalPage());
    });
}

String FirstWifiProvisioning::buildResultPage(const String& title, const String& body) const {
    String html;
    html.reserve(1024);
    html += "<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;background:#0e1a2b;color:#eaf2ff;padding:24px;} .card{max-width:560px;margin:auto;background:#13233a;border-radius:16px;padding:20px;} a{color:#8dc3ff;}</style>";
    html += "</head><body><div class='card'><h1>" + htmlEscape(title) + "</h1><p>" + htmlEscape(body) + "</p><p><a href='/'>Retour</a></p></div></body></html>";
    return html;
}

String FirstWifiProvisioning::buildPortalPage() const {
    String html;
    html.reserve(1600);
    html += "<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;background:#0e1a2b;color:#eaf2ff;padding:24px;} .card{max-width:560px;margin:auto;background:#13233a;border-radius:16px;padding:20px;} input{width:100%;padding:10px;margin:8px 0;border-radius:10px;border:1px solid #2d4a73;background:#0b1422;color:#fff;} button{padding:10px 16px;border-radius:10px;border:0;background:#4da3ff;color:#fff;font-weight:700;} small{color:#8faed0;}</style>";
    html += "</head><body><div class='card'>";
    html += "<h1>PicoWake WiFi setup</h1>";
    html += "<p>Le wifi n'a pas ete trouve. Renseigne un reseau pour le premier demarrage.</p>";
    html += "<form action='/save' method='get'>";
    html += "<input name='ssid' placeholder='SSID' value='" + htmlEscape(savedSsid_) + "'>";
    html += "<input name='pass' placeholder='Mot de passe' type='password' value=''>";
    html += "<button type='submit'>Sauvegarder et connecter</button>";
    html += "</form>";
    html += "<p><small>AP: " + htmlEscape(apSsid_) + "</small></p>";
    html += "</div></body></html>";
    return html;
}

bool FirstWifiProvisioning::isPortalActive() const {
    return portalActive_;
}

bool FirstWifiProvisioning::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool FirstWifiProvisioning::isConnecting() const {
    return connecting_;
}

String FirstWifiProvisioning::statusMessage() const {
    return statusMessage_;
}

String FirstWifiProvisioning::ssid() const {
    return WiFi.SSID();
}

IPAddress FirstWifiProvisioning::localIP() const {
    return WiFi.localIP();
}

String FirstWifiProvisioning::htmlEscape(const String& value) {
    String out;
    out.reserve(value.length() + 16);
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

String FirstWifiProvisioning::urlDecode(const String& value) {
    String out;
    out.reserve(value.length());
    for (int i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < value.length()) {
            const char hex[3] = { value[i + 1], value[i + 2], '\0' };
            out += static_cast<char>(strtol(hex, nullptr, 16));
            i += 2;
        } else {
            out += c;
        }
    }
    return out;
}

void FirstWifiProvisioning::loop() {
    if (connecting_) {
        if (WiFi.status() == WL_CONNECTED) {
            connecting_ = false;
            statusMessage_ = "WiFi: connecte a " + savedSsid_;
            WIFI_LOG("async connect success ip=%s", WiFi.localIP().toString().c_str());
        } else if ((millis() - connectStartMs_) >= connectTimeoutMs_) {
            connecting_ = false;
            statusMessage_ = "WiFi: echec connexion";
            WIFI_LOG("async connect timeout status=%d", static_cast<int>(WiFi.status()));
        }
    }

    if (!portalActive_) {
        return;
    }

    dnsServer_.processNextRequest();
    server_.handleClient();
}
