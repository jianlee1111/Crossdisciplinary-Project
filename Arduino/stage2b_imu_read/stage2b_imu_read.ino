// 2단계 B: MPU-6050(GY-521) 가속도 읽기, ±4g -- 컴파일/실행 검증 안 됨
#include <Wire.h>
const uint8_t ADDR = 0x68;           // AD0=GND → 0x68, AD0=3.3V → 0x69
void wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ADDR); Wire.write(reg); Wire.write(val); Wire.endTransmission();
}
void setup() {
  Serial.begin(115200);
  Wire.begin(); Wire.setClock(100000);
  delay(100);                        // 전원 투입 후 100ms 대기
  wr(0x6B, 0x00);                    // PWR_MGMT_1: 절전 해제
  wr(0x1C, 0x08);                    // ACCEL_CONFIG: ±4g (8192 LSB/g)
  wr(0x1A, 0x01);                    // CONFIG: DLPF_CFG=1
  Wire.beginTransmission(ADDR); Wire.write(0x75); Wire.endTransmission(false);
  Wire.requestFrom(ADDR, (uint8_t)1);
  Serial.print("WHO_AM_I = 0x"); Serial.println(Wire.read(), HEX);  // 보통 0x68 (복제칩은 다를 수 있음)
}
void loop() {
  Wire.beginTransmission(ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(ADDR, (uint8_t)6);
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  Serial.print(ax / 8192.0, 3); Serial.print('\t');
  Serial.print(ay / 8192.0, 3); Serial.print('\t');
  Serial.println(az / 8192.0, 3);    // 가만히 두면 한 축이 약 ±1.0 (중력, 단위 g)
  delay(20);
}
