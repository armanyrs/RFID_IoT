// attendance_lcd_rfid.ino
// Fitur:
// - Baca nama dari block MIFARE (blockNum)
// - Kirim absensi (toggle) ke Apps Script Web App
// - Tampilkan Nama & Status (MASUK/KELUAR) di LCD I2C
// - Menangani redirect (HTTP 302) dengan setFollowRedirects
// - Parsing JSON sederhana tanpa ArduinoJson

#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// RFID Pin Mapping
#define SS_PIN    5
#define RST_PIN   4
#define BUZZER    15

// SPI Pins
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  17

// LCD I2C
#define LCD_SDA   8
#define LCD_SCL   9
#define LCD_ADDR  0x27   // ubah ke 0x3F jika modul kamu berbeda

// Block MIFARE tempat menyimpan nama (pastikan sudah di-write)
int blockNum = 2;
byte bufferLen = 18;
byte readBlockData[18];
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

// ================== GANTI: SSID & PASSWORD ==================
const char* WIFI_SSID     = "TRM";
const char* WIFI_PASSWORD = "12345678";  // kosong "" jika open
// ============================================================

// URL Web App (DEFINITIF - hasil Deploy). Jangan /edit, pakai /exec
const String SCRIPT_URL = "https://script.google.com/macros/s/AKfycbx0vqbN5wh4MM-bnjgpWIn4ykNZP_BD1oU4qvvR-AjVwndALZzF326rjpGzEAparZcdAQ/exec";

// Forward declarations
String sendAttendanceToggle(const String &nama);
String parseJsonField(const String &json, const String &field);
void beepMasuk();
void beepKeluar();
void beepError();
String urlEncode(const String &str);
void readBlock(int blockNum, byte readBlockData[]);

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nBooting...");

  // LCD init
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Init RFID...");
  lcd.setCursor(0,1); lcd.print("WiFi connect");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    Serial.print(".");
    delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK IP: " + WiFi.localIP().toString());
    lcd.clear(); lcd.print("WiFi OK");
  } else {
    Serial.println("\nWiFi gagal");
    lcd.clear(); lcd.print("WiFi Gagal");
  }

  // SPI + RFID
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SS_PIN);
  mfrc522.PCD_Init();
  delay(500);
  lcd.clear();
  lcd.print("Scan Kartu...");
  Serial.println("Siap scan kartu...");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  Serial.println("\n=== Kartu Terdeteksi ===");
  readBlock(blockNum, readBlockData);

  // Konversi block ke nama
  String nama = "";
  for (int i = 0; i < 16; i++) {
    char c = (char)readBlockData[i];
    if (c == 0 || c == 255) break;
    nama += c;
  }
  nama.trim();

  Serial.println("Nama: '" + nama + "'");
  if (nama.length() == 0) {
    Serial.println("Blok kosong / tidak ada nama.");
    lcd.clear(); lcd.print("Data kosong");
    beepError();
    delay(1500);
    lcd.clear(); lcd.print("Scan lagi...");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }

  // Kirim attendance (toggle)
  String statusStr = sendAttendanceToggle(nama);
  Serial.println("Status baru: " + statusStr);

  // Tampilkan LCD
  lcd.clear();
  String line1 = "Nama:" + nama;
  if (line1.length() > 16) line1 = line1.substring(0,16);
  lcd.setCursor(0,0); lcd.print(line1);

  String line2 = "Status:" + statusStr;
  if (line2.length() > 16) line2 = line2.substring(0,16);
  lcd.setCursor(0,1); lcd.print(line2);

  if (statusStr.equalsIgnoreCase("MASUK")) {
    beepMasuk();
  } else if (statusStr.equalsIgnoreCase("KELUAR")) {
    beepKeluar();
  } else {
    beepError();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(2500);
  lcd.clear(); lcd.print("Scan Kartu...");
}

// ================= RFID Read Block =================
void readBlock(int blockNum, byte readBlockData[]) {
  bufferLen = 18;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Auth gagal: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = mfrc522.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Gagal membaca: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  Serial.println("Block OK.");
}

// ================= Kirim Attendance =================
String sendAttendanceToggle(const String &nama) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi putus -> OFFLINE");
    return "OFFLINE";
  }

  WiFiClientSecure client;
  client.setInsecure(); // Prototipe, aman di LAN. Produksi: pakai root CA.

  String url = SCRIPT_URL + "?mode=attendance&nama=" + urlEncode(nama);
  Serial.println("URL: " + url);

  HTTPClient https;
  // Otomatis follow redirect (302 -> 200)
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  https.setTimeout(10000);

  String statusOut = "ERROR";
  if (https.begin(client, url)) {
    int httpCode = https.GET();
    Serial.printf("HTTP code: %d\n", httpCode);

    // Jika masih 302 dan library belum follow, coba manual redirect
    if (httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_MOVED_PERMANENTLY ||
        httpCode == HTTP_CODE_TEMPORARY_REDIRECT || httpCode == HTTP_CODE_PERMANENT_REDIRECT) {
      String loc = https.header("Location");
      Serial.println("Redirect Location: " + loc);
      https.end();
      if (loc.length()) {
        if (https.begin(client, loc)) {
          httpCode = https.GET();
          Serial.printf("HTTP code after redirect: %d\n", httpCode);
        }
      }
    }

    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      payload.trim();
      Serial.println("Payload: " + payload);
      // Ambil field "status"
      String st = parseJsonField(payload, "status");
      if (st.length() == 0 || st == "NOTFOUND") {
        // fallback ke field "status" tidak ada -> mungkin error
        st = "PARSEERR";
      }
      statusOut = st;
    } else {
      Serial.println("Non-200 (tidak final).");
      statusOut = "NETFAIL";
    }
    https.end();
  } else {
    Serial.println("HTTPS begin gagal");
    statusOut = "NETERR";
  }
  return statusOut;
}

// Parsing sederhana: cari "field":"nilai"
String parseJsonField(const String &json, const String &field) {
  String key = "\"" + field + "\"";
  int idx = json.indexOf(key);
  if (idx < 0) return "NOTFOUND";
  int colon = json.indexOf(":", idx);
  if (colon < 0) return "NOTFOUND";
  int q1 = json.indexOf("\"", colon + 1);
  if (q1 < 0) return "NOTFOUND";
  int q2 = json.indexOf("\"", q1 + 1);
  if (q2 < 0) return "NOTFOUND";
  String val = json.substring(q1 + 1, q2);
  val.trim();
  return val;
}

// ================= Buzzer Patterns =================
void beepMasuk() {
  digitalWrite(BUZZER, HIGH); delay(120);
  digitalWrite(BUZZER, LOW);  delay(120);
  digitalWrite(BUZZER, HIGH); delay(120);
  digitalWrite(BUZZER, LOW);
}

void beepKeluar() {
  digitalWrite(BUZZER, HIGH); delay(300);
  digitalWrite(BUZZER, LOW);
}

void beepError() {
  for (int i=0; i<3; i++) {
    digitalWrite(BUZZER, HIGH); delay(80);
    digitalWrite(BUZZER, LOW);  delay(80);
  }
}

// ================= URL Encoder =================
String urlEncode(const String &str) {
  String encoded = "";
  char c;
  char bufHex[4];
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ||
        ('0' <= c && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      sprintf(bufHex, "%%%02X", (unsigned char)c);
      encoded += bufHex;
    }
  }
  return encoded;
}
