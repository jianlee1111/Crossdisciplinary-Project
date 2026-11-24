# CDP 센서 로거 사용법 (arduino_logger.py) 

## 1. 준비 (처음 한 번)

1. 파이썬 설치 확인: 명령 프롬프트에서 `python --version`. 없으면 python.org에서 설치하고 "Add python.exe to PATH"를 체크한다.
2. pyserial 설치: `pip install pyserial`
3. 폴더 하나(예: `sensor_log`)를 만들고 `cdp_logger.py`를 넣는다. CSV도 이 폴더에 생긴다.
4. 아두이노 IDE의 도구 → 포트에서 포트 번호를 확인한다. COM3이 아니면 스크립트 맨 위 `PORT = "COM3"`를 고친다.

## 2. 스케치 설정 (stage6_integrated.ino)

| 상수 | 값 | 이유 |
|---|---|---|
| `PT` | 500000 | 온도 0.5초 간격 (요청 사양) |
| `PA` | 500 | FSR·피에조 2 kHz |
| `PB` | 2000 | IMU 500 Hz |

배선 확인용으로 셋 다 250000으로 둔 상태라면 위 값으로 바꿔 다시 업로드한다.

## 3. 실행 순서

1. 아두이노 시리얼 모니터를 닫는다. (포트는 한 프로그램만 열 수 있음)
2. `sensor_log` 폴더 주소창에 `cmd`를 치고 Enter → 명령 프롬프트가 열린다.
3. `python arduino_logger.py`
4. 묻는 순서대로 입력한다.

| 질문 | 예시 | 뜻 |
|---|---|---|
| session_id | `S01` | 실험 세션 |
| group_id | `S01_G001` | 같은 물 준비 조건의 반복 측정 묶음 |
| material | `0` | 컵 재질 (0 또는 1) |
| water_temp_C | `40.0` | 기준 온도계로 잰 실제 수온 |

5. Trial 진행

| 단계 | 누르는 키 | 일어나는 일 |
|---|---|---|
| Trial 시작 | Enter | baseline 기록 시작, trial_id 1 증가 |
| 5초쯤 대기 | — | 센서를 컵에서 뗀 상태로 둔다 |
| 컵에 대는 순간 | Enter | 다음 온도 측정부터 contact로 기록 |
| 15초 뒤 | — | 자동 종료, "저장 완료" 표시 |

6. 다음 Trial

| 키 | 동작 |
|---|---|
| Enter | 같은 조건으로 다음 Trial |
| `n` | 조건 다시 입력 (물을 새로 준비했을 때 group_id 변경) |
| `q` | 종료 |

## 4. 만들어지는 파일

세션마다 세 파일이 생기고 Trial을 반복하면 이어서 쌓인다.

| 파일 | 열 |
|---|---|
| `temp_<session>.csv` | trial_id, session_id, group_id, time_ms, phase, temp_C, material, water_temp_C |
| `fsr_piezo_<session>.csv` | trial_id, session_id, group_id, time_ms, phase, fsr, piezo1, piezo2 |
| `imu_<session>.csv` | trial_id, session_id, group_id, time_ms, phase, ax, ay, az, gx, gy, gz |

- `time_ms`: 그 Trial의 첫 온도 측정 시각을 0으로 한 경과 시간. 세 파일이 같은 기준을 쓰므로 trial_id + time_ms로 합칠 수 있다.
- `phase`: `baseline` 또는 `contact`.
- 단위: fsr·piezo는 0~16383(14비트), 가속도 g, 각속도 deg/s, 온도 °C.
- Trial당 온도 행: baseline 약 10개(누르는 타이밍에 따라 달라짐) + contact 31개.

## 5. 주의

- baseline 길이는 두 번째 Enter를 누르는 타이밍으로 정해진다. 정확히 5초가 아니어도 된다.
- 온도를 못 읽은 행은 `NaN`으로 남는다. 임의 값으로 채우지 않는다.
- 프로그램을 껐다 켜도 같은 세션 파일이 있으면 Trial 번호가 이어진다.
- Trial 사이에 열전대가 식을 시간을 준다. 공기 중 냉각 시정수는 약 27초(추정)라 80초쯤 쉬거나 실온 금속에 대서 식힌다.
- 업로드할 때는 스크립트를 끈다.

## 6. 자주 나는 에러

| 메시지 | 원인과 해결 |
|---|---|
| `could not open port 'COM3'` | 시리얼 모니터가 열려 있거나 포트 번호가 다름 |
| `'python'은(는) 내부 또는 외부 명령...` | 설치 시 PATH 체크 누락 → 재설치 |
| `No module named 'serial'` | `pip install pyserial` 안 함 |
| contact에서 안 끝남 | 보드에서 T 줄이 안 나오는 상태. 스케치의 `USE_TC`가 1인지, 열전대 배선 확인 |
| 파일이 헤더만 있음 | 보드에 통합 스케치가 아닌 다른 스케치가 올라가 있음 |
