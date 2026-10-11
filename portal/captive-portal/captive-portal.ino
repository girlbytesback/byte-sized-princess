// pocket vault — encrypted version of the server-charms-workshop sketch, for a Seeed XIAO ESP32C6.
// replaces portal/captive-portal/captive-portal.ino. files that live next to it:
//   web-content.h   the page (paste template-vault.html between the rawliteral markers)
//   vault-crypto.h  the two crypto routines
//   vault-blob.h    your encrypted vault, written by encrypt_vault.py (never commit this one)
// arduino ide: board = XIAO_ESP32C6 (esp32 core 3.x), tools -> USB CDC On Boot -> Enabled.
//
// how it works:
//   the vault sits in flash encrypted (AES-256-GCM). when you type your vault passphrase on
//   the page, it's POSTed to /unlock; the chip re-derives the key from it (PBKDF2, ~1-2 s),
//   decrypts, sends the list back as json, and zeroes its buffers. nothing decrypted is ever
//   written to flash and nothing stays in ram after the response. wrong passphrases back off:
//   2 s, 4 s, 8 s... up to 10 minutes, and the counter survives a reset.
//
// vs the workshop version: wifi password, one client, boring ssid, low tx power,
//   Cache-Control: no-store, deep sleep after 3 idle minutes (press reset to wake;
//   the usb port is gone while it sleeps, so wake it before uploading).

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include "web-content.h"
#include "vault-crypto.h"
#include "vault-blob.h"

// ---- settings --------------------------------------------------------------
const char *ssid = "byte sized portal <3";                       // anything forgettable
const char *wifiPass = "";   // the WIFI password: 8-63 chars or the AP won't start
const int maxClients = 1;                               // just your phone
const unsigned long idleMs = 3UL * 60UL * 1000UL;       // sleep after 3 min with no clients
const unsigned long maxBackoffMs = 10UL * 60UL * 1000UL;
// ----------------------------------------------------------------------------

const IPAddress localIP(192, 168, 4, 1);
const IPAddress gatewayIP(192, 168, 4, 1);
const IPAddress subnetMask(255, 255, 255, 0);

DNSServer dnsServer;
WebServer server(80);
Preferences prefs;
unsigned long lastSeen = 0;
unsigned long lockedUntil = 0;
uint32_t fails = 0;

unsigned long backoffMs(uint32_t n)
{
  if (n == 0) return 0;
  unsigned long ms = 1000UL << (n > 20 ? 20 : n);      // 2 s, 4 s, 8 s ...
  return ms > maxBackoffMs ? maxBackoffMs : ms;
}

void noStore()
{
  server.sendHeader("Cache-Control", "no-store");
}

void handleRoot()
{
  noStore();
  // send_P streams the page out of flash in small chunks. plain send() would first copy
  // the whole page into ram, and with wifi running there isn't 150 kb of ram to spare.
  server.send_P(200, "text/html", index_html);
}

void handleNotFound()
{
  // captive-portal trick: every unknown URL (captive.apple.com etc.) bounces to us
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.send(302, "text/plain", "");
}

void handleUnlock()
{
  noStore();
  if (millis() < lockedUntil)
  {
    server.send(429, "application/json", "{\"wait\":" + String((lockedUntil - millis()) / 1000 + 1) + "}");
    return;
  }
  String pw = server.arg("plain");   // the raw POST body
  pw.trim();
  if (pw.length() == 0)
  {
    server.send(400, "application/json", "{\"wait\":0}");
    return;
  }

  unsigned long t0 = millis();
  uint8_t key[32];
  uint8_t *ct = (uint8_t *)malloc(VAULT_CT_LEN);
  uint8_t *pt = (uint8_t *)malloc(VAULT_CT_LEN + 1);
  bool ok = ct && pt &&
            vault_pbkdf2((const uint8_t *)pw.c_str(), pw.length(), VAULT_SALT, sizeof VAULT_SALT, VAULT_ITER, key);
  if (ok)
  {
    memcpy(ct, VAULT_CT, VAULT_CT_LEN);           // hardware aes wants the input in ram
    ok = vault_decrypt(key, VAULT_NONCE, VAULT_TAG, ct, VAULT_CT_LEN, pt);
  }
  memset(key, 0, sizeof key);

  if (ok)
  {
    pt[VAULT_CT_LEN] = 0;
    fails = 0;
    prefs.putUInt("fails", 0);
    server.send(200, "application/json", (const char *)pt);
  }
  else
  {
    fails++;
    prefs.putUInt("fails", fails);
    lockedUntil = millis() + backoffMs(fails);
    server.send(401, "application/json", "{\"wait\":" + String(backoffMs(fails) / 1000) + "}");
  }
  if (pt) { memset(pt, 0, VAULT_CT_LEN + 1); free(pt); }
  if (ct) { free(ct); }
  Serial.printf("unlock %s in %lu ms (fails=%u)\n", ok ? "ok" : "FAILED", millis() - t0, fails);
}

void goToSleep()
{
  Serial.println("nobody connected for a while, sleeping. press reset to wake.");
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  esp_deep_sleep_start(); // no wake source configured = stays asleep until reset
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  prefs.begin("vault", false);
  fails = prefs.getUInt("fails", 0);
  lockedUntil = millis() + backoffMs(fails);   // a reset doesn't skip the wait

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(localIP, gatewayIP, subnetMask);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  // (ssid, password, channel, hidden?, max clients)
  if (WiFi.softAP(ssid, wifiPass, 6, false, maxClients))
  {
    Serial.println("AP up: " + String(ssid));
  }
  else
  {
    Serial.println("AP failed to start (wifi password shorter than 8 chars?)");
  }

  dnsServer.start(53, "*", localIP);
  server.on("/", handleRoot);
  server.on("/unlock", HTTP_POST, handleUnlock);
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
