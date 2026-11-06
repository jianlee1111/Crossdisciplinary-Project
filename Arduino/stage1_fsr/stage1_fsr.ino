// 1단계: FSR + 10k 분압, A0 읽기 (UNO R4 Minima)  -- 컴파일/실행 검증 안 됨
void setup() {
  Serial.begin(115200);
  analogReadResolution(14);          // R4는 기본 10bit, 14bit로 올림 (0~16383)
}
void loop() {
  int raw = analogRead(A0);
  float volt = raw * 5.0 / 16383.0;
  Serial.print(raw); Serial.print('\t'); Serial.println(volt, 3);  // 시리얼 플로터에는 raw만 보고 싶으면 한 값만 출력
  delay(50);
}
