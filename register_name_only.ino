/*
  register_name_only.ino
  Hanya menulis Nama ke block 2 (16 byte) MIFARE Classic.
*/
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 4
#define SS_PIN  5
#define SPI_SCK   18
#define SPI_MISO  19
#define SPI_MOSI  17

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
byte writeBuf[16];
byte readBuf[18];
byte readLen = 18;

void build(const String &s) {
  for (int i=0;i<16;i++) writeBuf[i]=0x00;
  int n = s.length(); if (n>16) n=16;
  for (int i=0;i<n;i++) writeBuf[i]=(byte)s[i];
}

String readLine(const char* prompt){
  Serial.print(prompt);
  while(!Serial.available()) delay(5);
  String r = Serial.readStringUntil('\n');
  r.trim();
  return r;
}

void setup(){
  Serial.begin(115200);
  Serial.println("=== Registrasi Nama (Block 2) ===");
  Serial.println("Ketik nama lalu ENTER, kemudian tempelkan kartu.");

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SS_PIN);
  mfrc522.PCD_Init();
  for(byte i=0;i<6;i++) key.keyByte[i]=0xFF;
}

void loop(){
  if (Serial.available()) {
    String nama = Serial.readStringUntil('\n');
    nama.trim();
    if (!nama.length()) return;
    Serial.println("Menunggu kartu...");
    while(!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) delay(25);

    build(nama);

    // Auth + write
    MFRC522::StatusCode st = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
                                                      2, &key, &(mfrc522.uid));
    if (st != MFRC522::STATUS_OK){
      Serial.println("Auth gagal: " + String(mfrc522.GetStatusCodeName(st)));
    } else {
      st = mfrc522.MIFARE_Write(2, writeBuf, 16);
      if (st == MFRC522::STATUS_OK) Serial.println("Tulis OK");
      else Serial.println("Write gagal: " + String(mfrc522.GetStatusCodeName(st)));
    }

    // Baca balik
    readLen=18;
    st = mfrc522.MIFARE_Read(2, readBuf, &readLen);
    if (st == MFRC522::STATUS_OK){
      Serial.print("Isi block 2: ");
      for(int i=0;i<16;i++){
        if(readBuf[i]==0) break;
        Serial.print((char)readBuf[i]);
      }
      Serial.println();
    } else {
      Serial.println("Read gagal: " + String(mfrc522.GetStatusCodeName(st)));
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    Serial.println("Selesai. Input nama baru untuk kartu berikutnya.");
  }
}
