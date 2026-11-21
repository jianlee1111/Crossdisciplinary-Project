// CDP 온도 Trial 로거 (K형 열전대 + MAX6675) -- 컴파일/실행 검증 안 됨
// 출력: trial_id,session_id,group_id,time_ms,phase,temp_C,material,water_temp_C
//
// 시리얼 명령 (한 줄 입력 후 Enter):
//   s S01        session_id 설정
//   g S01_G001   group_id 설정
//   m 0          material 설정 (0 또는 1)
//   w 40.0       water_temp_C 설정 (기준 온도계 값)
//   h            CSV 헤더 한 줄 출력
//   b            Trial 시작 (baseline 측정 시작, trial_id 자동 증가)
//   c            접촉 시작 (다음 측정부터 contact, 31개 찍고 자동 종료)
// Trial 진행 중에는 c 외의 명령은 무시한다.
#include <AlashThermocouple.h>

AlashThermocouple tc(AlashThermocouple::MAX6675, 5, 6, 4);   // 칩, CS, SCK, SO

const uint32_t PERIOD_MS = 500;        // 측정 간격
const int CONTACT_SAMPLES = 31;        // 접촉 15초 (시작·종료 시점 포함)

String sessionId = "S01", groupId = "S01_G001";
int material = 0;
float waterTemp = 0.0;
int trialNo = 0;

enum State { IDLE, BASELINE, CONTACT };
State state = IDLE;
bool contactReq = false;
uint32_t t0, nextSample;
int contactCount;
String cmd;

void printHeader() {
  Serial.println(F("trial_id,session_id,group_id,time_ms,phase,temp_C,material,water_temp_C"));
}

void handle(String s) {
  s.trim();
  if (s.length() == 0) return;
  char c = s[0];
  String arg = s.substring(1);
  arg.trim();
  if (state != IDLE) {                         // Trial 중에는 접촉 명령만 받음
    if (c == 'c' && state == BASELINE) contactReq = true;
    return;
  }
  if (c == 'h') printHeader();
  else if (c == 's') sessionId = arg;
  else if (c == 'g') groupId = arg;
  else if (c == 'm') material = arg.toInt();
  else if (c == 'w') waterTemp = arg.toFloat();
  else if (c == 'b') {
    trialNo++;
    contactReq = false;
    t0 = millis();
    nextSample = t0;
    state = BASELINE;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}                           // PC가 포트를 열 때까지 대기
  tc.begin();
  printHeader();
}

void loop() {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r' || ch == '\n') { handle(cmd); cmd = ""; }
    else cmd += ch;
  }

  if (state != IDLE && (int32_t)(millis() - nextSample) >= 0) {
    nextSample += PERIOD_MS;
    if (state == BASELINE && contactReq) { state = CONTACT; contactCount = 0; }

    uint32_t t = millis() - t0;                // 실제 측정 시각
    float c = tc.temperature();                // °C, 오류면 NAN

    char id[8];
    snprintf(id, sizeof(id), "T%03d", trialNo);
    Serial.print(id); Serial.print(',');
    Serial.print(sessionId); Serial.print(',');
    Serial.print(groupId); Serial.print(',');
    Serial.print(t); Serial.print(',');
    Serial.print(state == CONTACT ? F("contact") : F("baseline")); Serial.print(',');
    if (c == c) Serial.print(c, 2); else Serial.print(F("NaN"));   // 누락값은 대체하지 않음
    Serial.print(',');
    Serial.print(material); Serial.print(',');
    Serial.println(waterTemp, 1);

    if (state == CONTACT && ++contactCount >= CONTACT_SAMPLES) state = IDLE;
  }
}
