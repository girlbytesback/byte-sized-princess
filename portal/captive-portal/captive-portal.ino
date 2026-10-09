// password-charm version of the server-charms-workshop sketch, for a Seeed XIAO ESP32C6.
// drop this in over portal/captive-portal/captive-portal.ino (web-content.h stays next to it).
// arduino ide: board = XIAO_ESP32C6 (esp32 core 3.x), tools -> USB CDC On Boot -> Enabled.
// the XIAO_ESP32C6 board variant switches on the built-in antenna by itself; nothing to do here.
//
// what changed vs the workshop version:
//   - wifi has a WPA2 passphrase and only lets ONE device join
//   - boring SSID (the original is designed to invite strangers; this one shouldn't)
//   - low transmit power: your phone is in your hand anyway. saves battery and
//     shrinks the radius anyone could connect from
//   - Cache-Control: no-store so the phone's browser doesn't keep a copy of the page
//   - deep sleep (~15 uA on the XIAO C6) after a few minutes with nobody connected.
//     press the XIAO's reset button to wake it. the usb port is gone while it sleeps,
//     so wake it before uploading new code.

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_sleep.h>
#include "web-content.h"

// ---- settings --------------------------------------------------------------
const char *ssid = "HP-Print-7C";                       // anything forgettable
const char *wifiPass = "change-me-to-something-long";   // 8-63 chars or the AP won't start
const int maxClients = 1;                               // just your phone
const unsigned long idleMs = 3UL * 60UL * 1000UL;       // sleep after 3 min with no clients
// ----------------------------------------------------------------------------

const IPAddress localIP(192, 168, 4, 1);
const IPAddress gatewayIP(192, 168, 4, 1);
const IPAddress subnetMask(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);
unsigned long lastSeen = 0;

void handleRoot()
{
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", index_html);
}

void handleNotFound()
{
  // captive-portal trick: every unknown URL (captive.apple.com etc.) bounces to us
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.send(302, "text/plain", "");
}

void goToSleep()
{
  Serial.println("nobody connected for a while, sleeping. RST or power-cycle to wake.");
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  esp_deep_sleep_start(); // no wake source configured = stays asleep until reset
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(localIP, gatewayIP, subnetMask);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // (ssid, passphrase, channel, hidden?, max clients)
  if (WiFi.softAP(ssid, wifiPass, 6, false, maxClients))
  {
    Serial.println("AP up: " + String(ssid));
  }
  else
  {
    Serial.println("AP failed to start (passphrase shorter than 8 chars?)");
  }

  dnsServer.start(53, "*", localIP);
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  lastSeen = millis();
}

void loop()
{
  dnsServer.processNextRequest();
  server.handleClient();

  if (WiFi.softAPgetStationNum() > 0)
  {
    lastSeen = millis();
  }
  if (millis() - lastSeen > idleMs)
  {
    goToSleep();
  }

  delay(50);
}
