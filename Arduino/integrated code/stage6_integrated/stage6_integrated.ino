// 6단계: 통합 스트림 (FSR + 피에조x2 + IMU + 열전대, FDC 제외) -- 컴파일/실행 검증 안 됨
// 센서별 켜기/끄기: 안 되는 센서는 0으로 두고 나머지만 돌리세요.
#define USE_FSR    1   // A0
#define USE_PIEZO  1   // 피에조 필름 직접 연결: (+) -> A1, (-) -> GND, 저항 병렬 (piezo2 column is unused, prints 0)
#define USE_IMU    1   // GY-521 0x68 (I2C)
#define USE_TC     1   // MAX6675: AlashThermocouple 라이브러리, CS=D5, SCK=D6, SO=D4 (이 조합만 동작 확인됨)
#include <Wire.h>
#if USE_IMU
#include <MPU6050_light.h>
MPU6050 mpu(Wire);
bool imuOk = false;
#endif
#if USE_TC
#include <AlashThermocouple.h>
AlashThermocouple tc(AlashThermocouple::MAX6675, 5, 6, 4);   // 칩, CS, SCK, SO
#endif

const uint32_t PA = 250000;   // A 스트림 주기 4 Hz (us) -- 배선 확인용, 원래 500 (2 kHz)
const uint32_t PB = 250000;   // B 스트림 주기 4 Hz -- 배선 확인용, 원래 2000 (500 Hz)
const uint32_t PT = 250000;   // T 스트림 주기 4 Hz
uint32_t nextA, nextB, nextT;
uint32_t cntA = 0, cntB = 0, cntT = 0, lastStat = 0;

bool due(uint32_t &next, uint32_t period) {
  int32_t d = (int32_t)(micros() - next);
  if (d < 0) return false;
  next += period;
  if (d > (int32_t)(10 * period)) next = micros() + period;   // 너무 밀리면 다시 맞춤
  return true;
}
void setup() {
  Serial.begin(115200);
  analogReadResolution(14);
#if USE_IMU
  Wire.begin();
  byte st = mpu.begin();
  if (st != 0) Serial.println("# MPU6050 연결 실패 (B 스트림 없음)");
  else {
    Serial.println("# 캘리브레이션 중... 센서를 움직이지 마세요");
    delay(1000);
    mpu.calcOffsets();            // 정지 상태에서 오프셋 보정
    imuOk = true;
  }
#endif
#if USE_TC
  if (!tc.begin()) Serial.println(tc.errorText());
#endif
  Serial.println("# format A: t_us,fsr,piezo1,piezo2 | B: t_us,ax(g),ay,az,gx(deg/s),gy,gz | T: t_us,tempC | S: stats");
  nextA = nextB = nextT = micros() + 100000;
  lastStat = millis();
}
void loop() {
  char buf[96];
#if USE_FSR || USE_PIEZO
  if (due(nextA, PA)) {
    int f = 0, p1 = 0, p2 = 0;
#if USE_FSR
    f = analogRead(A0);
#endif
#if USE_PIEZO
    p1 = analogRead(A1); p2 = 0;
#endif
    snprintf(buf, sizeof(buf), "A,%lu,%d,%d,%d\n", (unsigned long)micros(), f, p1, p2);
    Serial.write(buf); cntA++;
  }
#endif
#if USE_IMU
  if (imuOk && due(nextB, PB)) {
    uint32_t t = micros();
    mpu.update();
    Serial.print("B,"); Serial.print(t); Serial.print(',');
    Serial.print(mpu.getAccX(), 3); Serial.print(','); Serial.print(mpu.getAccY(), 3); Serial.print(',');
    Serial.print(mpu.getAccZ(), 3); Serial.print(',');
    Serial.print(mpu.getGyroX(), 2); Serial.print(','); Serial.print(mpu.getGyroY(), 2); Serial.print(',');
    Serial.println(mpu.getGyroZ(), 2);
    cntB++;
  }
#endif
#if USE_TC
  if (due(nextT, PT)) {
    uint32_t t = micros();
    float c = tc.temperature();                        // °C, 오류면 NAN
    Serial.print("T,"); Serial.print(t); Serial.print(',');
    if (c == c) Serial.println(c, 2); else Serial.println("NaN");
    cntT++;
  }
#endif
  if (millis() - lastStat >= 5000) {                       // 5초마다 실제 속도 출력
    snprintf(buf, sizeof(buf), "S,%lu,A=%lu,B=%lu,T=%lu per 5s\n", (unsigned long)millis(),
             (unsigned long)cntA, (unsigned long)cntB, (unsigned long)cntT);
    Serial.write(buf);
    cntA = cntB = cntT = 0; lastStat += 5000;
  }
}
