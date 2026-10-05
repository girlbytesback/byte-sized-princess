#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <esp_pm.h>
#include "web-content.h"

// Change how the WIFI shows up in the list of available WIFIs
const char *ssid = "look around";
// Max number of people that can connect to the WiFi
const int maxClients = 10;

const IPAddress localIP(192, 168, 4, 1);
const IPAddress gatewayIP(192, 168, 4, 1);
const IPAddress subnetMask(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);

void handleRoot()
{
  // Serve the website imported from web-content.h
  server.send(200, "text/html", index_html);
}

void handleNotFound()
{
  // Redirect any unknown path to root
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.send(302, "text/plain", "");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Optional WIFI power saving mode
  // esp_pm_config_esp32c3_t pm_config = {
  // .max_freq_mhz = 80,
  // .min_freq_mhz = 10,
  // .light_sleep_enable = true};
  // esp_pm_configure(&pm_config);

  // Setup WiFi AP with reduced power
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

  // Reduced WiFi transmission power to save energy
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  if (WiFi.softAP(ssid, nullptr, 6, false, maxClients))
  {
    // Successfully started AP
    Serial.println("WiFi AP started: " + String(ssid));
  }
  else
  {
    // Error starting AP
    Serial.println("WiFi AP failed to start");
  }

  // Optional WIFI power saving mode
  // esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // Enable modem sleep

  // Start DNS server
  dnsServer.start(53, "*", localIP);

  // Setup web server routes
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop()
{
  // Process requests with minimal delay
  dnsServer.processNextRequest();
  server.handleClient();

  // Longer delay to allow CPU to enter light sleep more often
  delay(50);
}