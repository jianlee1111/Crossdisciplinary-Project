// 7단계: 통합 스트림, 타이머 인터럽트 샘플링 (FSR + 피에조 + IMU + 열전대, FDC 제외) -- 컴파일/실행 검증 안 됨
// 피에조 4 kHz·FSR 100 Hz는 타이머 인터럽트에서 정확한 간격으로 읽고, IMU 100 Hz·온도 2 Hz는 loop에서 읽는다.
// 모든 줄의 t_us는 타이머 틱 기준 시각(틱 번호 x 250 us)이라 네 스트림의 시간축이 같다.
// 센서별 켜기/끄기: 안 되는 센서는 0으로 두고 나머지만 돌리세요.
#define USE_FSR    1   // A0
#define USE_PIEZO  1   // 피에조 필름 -> A1
#define USE_IMU    1   // GY-521 0x68 (I2C)
#define USE_TC     1   // MAX6675: AlashThermocouple 라이브러리, CS=D5, SCK=D6, SO=D4 (이 조합만 동작 확인됨)
#include <Wire.h>
#include <FspTimer.h>
#if USE_IMU
#include <MPU6050_light.h>
MPU6050 mpu(Wire);
bool imuOk = false;
#endif
#if USE_TC
#include <AlashThermocouple.h>
AlashThermocouple tc(AlashThermocouple::MAX6675, 5, 6, 4);   // 칩, CS, SCK, SO
#endif

const uint32_t TICK_US = 250;     // 타이머 주기 = 피에조 4 kHz
const uint16_t BLK = 40;          // P 줄 하나에 피에조 40개 (10 ms 분량)
const uint16_t FSR_DIV = 40;      // FSR = 4000 / 40 = 100 Hz
const uint32_t PB = 10000;        // B 스트림(IMU) 주기 100 Hz (us)
const uint32_t PT = 500000;       // T 스트림(온도) 주기 2 Hz
const uint8_t NB = 32;            // 피에조 버퍼 블록 수 (320 ms 분량)

FspTimer tmr;
volatile uint16_t blk[NB][BLK];
volatile uint32_t blkTick[NB];    // 각 블록 첫 샘플의 틱 번호
volatile uint8_t wr = 0, rd = 0;
volatile uint32_t tick = 0, dropBlk = 0;
volatile uint16_t fsrVal = 0;
volatile uint32_t fsrTick = 0;
volatile bool fsrNew = false;

uint32_t nextB, nextT;
uint32_t cntP = 0, cntA = 0, cntB = 0, cntT = 0, lastStat = 0;

void onTick(timer_callback_args_t *) {
  uint32_t n = tick++;
#if USE_PIEZO
  uint16_t i = n % BLK;
  if (i == 0) blkTick[wr] = n;
  blk[wr][i] = analogRead(A1);
  if (i == BLK - 1) {
    uint8_t nx = (wr + 1) % NB;
    if (nx != rd) wr = nx; else dropBlk++;     // 버퍼가 차면 이 블록을 버리고 같은 자리에 다시 씀
  }
#endif
#if USE_FSR
  if (n % FSR_DIV == 0) { fsrVal = analogRead(A0); fsrTick = n; fsrNew = true; }
#endif
}

bool startTimer(float hz) {
  uint8_t type = GPT_TIMER;
  int8_t ch = FspTimer::get_available_timer(type);
  if (ch < 0) ch = FspTimer::get_available_timer(type, true);   // 남는 게 없으면 PWM용 타이머 사용
  if (ch < 0) return false;
  if (!tmr.begin(TIMER_MODE_PERIODIC, type, ch, hz, 0.0f, onTick)) return false;
  if (!tmr.setup_overflow_irq(2)) return false;                 // 우선순위 2 (기본 12): USB·I2C 인터럽트에 밀려 틱을 놓치지 않게
  if (!tmr.open()) return false;
  return tmr.start();
}

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
  Serial.println("# format P: t_us,piezo x40 (250 us 간격) | A: t_us,fsr | B: t_us,ax(g),ay,az,gx(deg/s),gy,gz | T: t_us,tempC | S: stats");
  nextB = nextT = micros() + 100000;
  lastStat = millis();
  if (!startTimer(1000000.0f / TICK_US)) Serial.println("# 타이머 시작 실패 (P, A 스트림 없음)");
}
void loop() {
  char buf[96];
#if USE_PIEZO
  if (rd != wr) {                                          // 다 찬 블록이 있으면 한 줄로 출력
    char line[320];
    int n = snprintf(line, sizeof(line), "P,%lu", (unsigned long)(blkTick[rd] * TICK_US));
    for (uint16_t i = 0; i < BLK; i++) n += snprintf(line + n, sizeof(line) - n, ",%u", blk[rd][i]);
    line[n++] = '\n';
    Serial.write(line, n);
    rd = (rd + 1) % NB;
    cntP += BLK;
  }
#endif
#if USE_FSR
  if (fsrNew) {
    noInterrupts();
    uint16_t f = fsrVal; uint32_t ft = fsrTick; fsrNew = false;
    interrupts();
    snprintf(buf, sizeof(buf), "A,%lu,%u\n", (unsigned long)(ft * TICK_US), f);
    Serial.write(buf); cntA++;
  }
#endif
#if USE_IMU
  if (imuOk && due(nextB, PB)) {
    uint32_t t = tick * TICK_US;
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
    uint32_t t = tick * TICK_US;
    float c = tc.temperature();                        // °C, 오류면 NAN
    Serial.print("T,"); Serial.print(t); Serial.print(',');
    if (c == c) Serial.println(c, 2); else Serial.println("NaN");
    cntT++;
  }
#endif
  if (millis() - lastStat >= 5000) {                       // 5초마다 실제 속도 출력
    snprintf(buf, sizeof(buf), "S,%lu,P=%lu,A=%lu,B=%lu,T=%lu per 5s,drop_blocks=%lu\n", (unsigned long)millis(),
             (unsigned long)cntP, (unsigned long)cntA, (unsigned long)cntB, (unsigned long)cntT, (unsigned long)dropBlk);
    Serial.write(buf);
    cntP = cntA = cntB = cntT = 0; lastStat += 5000;
  }
}
