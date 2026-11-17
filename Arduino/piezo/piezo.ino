// 4-A 피에조 반응 확인: A2 읽기 (14비트) -- 컴파일/실행 검증 안 됨
void setup() {
  Serial.begin(115200);
  analogReadResolution(14);
}
void loop() {
  Serial.println(analogRead(A1));   // 가만히 약 8192, 톡 치면 위아래로 튐
  delay(2);
}
