#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 5     // SDA RFID
#define RST_PIN 4     // RST RFID

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

int blockNum = 2; // blok tempat menyimpan data (harus <16 byte)

void setup() {
  Serial.begin(9600);
  Serial.println("Tempelkan kartu untuk MENULIS data...");

  // SPI sesuai ESP32-S3
  SPI.begin(18, 19, 17, SS_PIN); // SCK, MISO, MOSI, SS

  mfrc522.PCD_Init();

  // Key default kartu baru
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
}

void loop() {

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  Serial.println("\n=== KARTU TERDETEKSI ===");

  // Data yang akan ditulis ke blok (max 16 char)
  String data = "FUADY";
  if (data.length() > 16) data = data.substring(0, 16);

  byte dataBlock[16];
  memset(dataBlock, 0, 16);
  data.getBytes(dataBlock, 16);

  // Autentikasi blok
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A,
      blockNum,
      &key,
      &(mfrc522.uid)
  );

  if (status != MFRC522::STATUS_OK) {
    Serial.print("Auth gagal: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // Tulis data
  status = mfrc522.MIFARE_Write(blockNum, dataBlock, 16);

  if (status == MFRC522::STATUS_OK) {
    Serial.println("✅ BERHASIL: Data tersimpan ke kartu!");
  } else {
    Serial.print("❌ Gagal menulis: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(2000);
}
