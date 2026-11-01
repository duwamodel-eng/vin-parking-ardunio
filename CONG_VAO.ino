#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// =========================
// RFID
// =========================
#define RFID_SS   27
#define RFID_RST  26

#define RFID_SCK  25
#define RFID_MISO 32
#define RFID_MOSI 33

MFRC522 rfid(RFID_SS, RFID_RST);

// =========================
// LCD
// =========================
#define LCD_SDA 21
#define LCD_SCL 22

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =========================
// SERVO + BUZZER
// =========================
#define SERVO_PIN  18
#define BUZZER_PIN 23

Servo barrier;

// =========================
// UID HỢP LỆ
// =========================
byte validUID[] = {
  0x3E, 0x51, 0xC9, 0x06
};

const byte UID_SIZE = 4;

// =========================
// KIỂM TRA UID
// =========================
bool checkUID() {

  if (rfid.uid.size != UID_SIZE) {
    return false;
  }

  for (byte i = 0; i < UID_SIZE; i++) {
    if (rfid.uid.uidByte[i] != validUID[i]) {
      return false;
    }
  }

  return true;
}

// =========================
// BEEP
// =========================
void beep(int times, int duration = 120) {

  for (int i = 0; i < times; i++) {

    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);

    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// =========================
// MỞ BARRIER
// =========================
void openBarrier() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("THE HOP LE");

  lcd.setCursor(0, 1);
  lcd.print("MO BARRIER");

  beep(1);

  // Mở từ từ
  for (int angle = 90; angle >= 0; angle -= 3) {
    barrier.write(angle);
    delay(20);
  }

  delay(3000);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("DANG DONG");

  // Đóng từ từ
  for (int angle = 0; angle <= 90; angle += 3) {
    barrier.write(angle);
    delay(20);
  }

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("VIN PARKING");

  lcd.setCursor(0, 1);
  lcd.print("QUET THE RFID");
}

// =========================
// TỪ CHỐI
// =========================
void rejectCard() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("THE KHONG HOP LE");

  lcd.setCursor(0, 1);
  lcd.print("TU CHOI!");

  beep(3, 100);

  delay(2000);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("VIN PARKING");

  lcd.setCursor(0, 1);
  lcd.print("QUET THE RFID");
}


String readPiCommand(unsigned long timeoutMs = 35000) {
  String line;
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (Serial.available()) {
      char c = Serial.read();

      if (c == '\n') {
          line.trim();

          if (line == "RESULT,QUEUED") {
              line = "";
              continue;
          }
          return line;
      }      

      if (c != '\r' && line.length() < 100) {
        line += c;
      }
    }

    delay(2);
  }

  return "";
}

// =========================
// SETUP
// =========================
void setup() {

  Serial.begin(115200);

  // RFID SPI
  SPI.begin(
    RFID_SCK,
    RFID_MISO,
    RFID_MOSI,
    RFID_SS
  );

  rfid.PCD_Init();

  // LCD
  Wire.begin(LCD_SDA, LCD_SCL);

  lcd.init();
  lcd.backlight();

  // Servo
  barrier.attach(SERVO_PIN);
  barrier.write(90);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // LCD khởi động
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("VIN PARKING");

  lcd.setCursor(0, 1);
  lcd.print("QUET THE RFID");

  Serial.println("==============================");
  Serial.println("VIN PARKING - CONG VAO");
  Serial.println("==============================");
}

// =========================
// LOOP
// =========================

void loop() {
  if (!rfid.PICC_IsNewCardPresent()) {
    delay(20);
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    delay(20);
    return;
  }

  // Đọc UID thành chuỗi HEX
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }

    uid += String(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1) {
      uid += ":";
    }
  }

  uid.toUpperCase();

  // Gửi thẻ lên Raspberry Pi, không tự mở barrier
  Serial.print("CARD,");
  Serial.println(uid);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DANG XAC THUC");


String result = readPiCommand(330000);

if (result == "RESULT,REGISTERED") {
  // Chế độ đăng ký RFID: không mở barrier.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DA NHAN UID");
  lcd.setCursor(0, 1);
  lcd.print("DANG KY RFID");
  beep(1);
  delay(1500);

} else if (result == "RESULT,CARD_VALID") {
  // Thẻ hợp lệ: chờ quyết định xác thực khuôn mặt.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("THE HOP LE");
  lcd.setCursor(0, 1);
  lcd.print("CHO KHUON MAT");

  // Đọc kết quả thứ hai từ Raspberry Pi.
  result = readPiCommand(180000);
  Serial.printf("[PI RESULT 2] '%s' at %lu ms\n",
              result.c_str(), millis());

  if (result == "RESULT,ENTRY_ALLOWED") {
    openBarrier();
  } else if (result == "RESULT,FACE_DENIED") {
    Serial.println("[LCD] Display FACE_DENIED");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("KHUON MAT SAI");
    lcd.setCursor(0, 1);
    lcd.print("TU CHOI!");

    beep(3, 100);
    delay(3000);
  } else if (result == "RESULT,FACE_TIMEOUT") {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("HET THOI GIAN");
    lcd.setCursor(0, 1);
    lcd.print("QUET LAI THE");
    beep(2, 100);
    delay(2000);
  } else {
    // Không nhận được kết quả hợp lệ: đóng cổng.
    rejectCard();
  }

} else {
  // Thẻ sai, lỗi giao tiếp hoặc hết thời gian: đóng cổng.
  rejectCard();
}

lcd.clear();
lcd.setCursor(0, 0);
lcd.print("VIN PARKING");
lcd.setCursor(0, 1);
lcd.print("QUET THE RFID");

rfid.PICC_HaltA();
rfid.PCD_StopCrypto1();
delay(500);

}
