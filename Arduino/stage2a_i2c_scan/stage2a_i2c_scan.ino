// 2단계 A: I2C 주소 스캔 -- 컴파일/실행 검증 안 됨
#include <Wire.h>
void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(100000);             // 점검은 100kHz로 (최종은 400kHz)
  delay(500);
}
void loop() {
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print("found 0x"); Serial.println(a, HEX); n++;
    }
  }
  if (n == 0) Serial.println("nothing found");
  Serial.println("---");
  delay(1500);
}
