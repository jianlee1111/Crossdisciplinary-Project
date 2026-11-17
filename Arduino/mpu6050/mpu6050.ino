#include <Wire.h>
#include <MPU6050_light.h>

MPU6050 mpu(Wire);

void setup() {
  Serial.begin(9600);
  Wire.begin();

  byte status = mpu.begin();

  if (status != 0) {
    Serial.println("MPU6050 연결 실패!");
    while (1);
  }

  Serial.println("캘리브레이션 중... 센서를 움직이지 마세요!");
  delay(1000);
  mpu.calcOffsets(); // 기본 설정으로 캘리브레이션
  Serial.println("완료!");

  Serial.println("가속도(g) | 자이로(deg/s)");
  Serial.println("============================");
}

void loop() {
  // 센서 데이터 업데이트
  mpu.update();

  // 실제 물리량 직접 읽기 (라이브러리가 자동 변환)
  float accelX = mpu.getAccX();  // g 단위
  float accelY = mpu.getAccY();  // g 단위
  float accelZ = mpu.getAccZ();  // g 단위

  float gyroX = mpu.getGyroX();  // deg/s 단위
  float gyroY = mpu.getGyroY();  // deg/s 단위
  float gyroZ = mpu.getGyroZ();  // deg/s 단위

  // 출력
  Serial.print("Accel: ");
  Serial.print(accelX, 3); Serial.print("g, ");
  Serial.print(accelY, 3); Serial.print("g, ");
  Serial.print(accelZ, 3); Serial.println("g");

  Serial.print("Gyro: ");
  Serial.print(gyroX, 2); Serial.print("°/s, ");
  Serial.print(gyroY, 2); Serial.print("°/s, ");
  Serial.print(gyroZ, 2); Serial.println("°/s");

  Serial.println("---");
  delay(500);
}
