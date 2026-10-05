#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include "web-content.h"

const char *ssid = "nicole";

// Max number of people that can connect to the WiFi
const int maxClients = 5;

DNSServer dnsServer;
WebServer server(80);

const IPAddress localIP(192, 168, 4, 1);
const IPAddress gatewayIP(192, 168, 4, 1);
const IPAddress subnetMask(255, 255, 255, 0);

void handleRoot()
{
  String html = String(index_html);

  if (LittleFS.exists("/guestbook.txt"))
  {
    File f = LittleFS.open("/guestbook.txt", "r");
    while (f.available())
    {
      html += "<div class='entry'>";
      html += f.readStringUntil('\n');
      html += "</div>";
    }
    f.close();
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleNotFound()
{
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleMessage()
{
  String name = server.arg("name");
  String msg = server.arg("msg");

  File f = LittleFS.open("/guestbook.txt", "a");
  f.println("<div class='name'>" + name + "</div>" + ": " + msg);
  f.close();

  server.sendHeader("Location", "/");
  server.send(303);
}

void setup()
{
  Serial.begin(115200);

  LittleFS.begin(true);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);
  WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

  dnsServer.start(53, "*", localIP);

  server.on("/", handleRoot);
  server.on("/send-message", HTTP_POST, handleMessage);
  server.onNotFound(handleNotFound);

  server.begin();
}

void loop()
{
  dnsServer.processNextRequest();
  server.handleClient();
}