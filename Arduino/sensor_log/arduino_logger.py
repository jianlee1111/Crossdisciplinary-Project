# cdp_logger.py -- 통합 스케치(stage7_integrated_timer.ino)의 시리얼 출력을 Trial 단위 CSV로 저장
# 실제 보드로는 검증 안 됨 (가짜 시리얼 데이터로만 시험함)
#
# Trial 하나 = 로봇 동작 시퀀스 한 번(114.5초). Enter를 누른 순간이 time_ms 0이고,
# 각 행의 phase는 아래 PHASES 표에 따라 경과 시간으로 붙인다.
#
# 만드는 파일 (세션마다, 이어쓰기):
#   temp_<session>.csv   trial_id,session_id,group_id,time_ms,phase,temp_C,material,water_temp_C   (2 Hz)
#   piezo_<session>.csv  trial_id,session_id,group_id,time_ms,phase,piezo                          (4 kHz)
#   fsr_<session>.csv    trial_id,session_id,group_id,time_ms,phase,fsr                            (100 Hz)
#   imu_<session>.csv    trial_id,session_id,group_id,time_ms,phase,ax,ay,az,gx,gy,gz              (100 Hz)
import bisect
import os
import threading
import serial

PORT = "COM3"
PIEZO_DT_US = 250           # P 줄 안의 샘플 간격 (스케치의 TICK_US와 같아야 함)

# 로봇 동작 시간표: (시작 시각 s, 이름). 로봇 쪽 시간표가 바뀌면 여기만 고친다.
PHASES = [
    (0.0, "open"), (3.0, "level"), (8.0, "reach_j3"), (14.0, "reach_j2"),
    (20.0, "j4_after"), (24.0, "pre_move"), (28.0, "settle"), (30.0, "squeeze"),
    (38.0, "hold"), (58.0, "rub_prep"), (62.0, "rub_settle"), (63.0, "rub"),
    (81.0, "regrip"), (85.0, "regrip_hold"), (87.0, "lift"), (93.0, "lift_hold"),
    (96.0, "shake"), (99.5, "lower"), (105.5, "release"), (108.5, "home"),
]
END_S = 114.5               # 마지막 동작(home)이 끝나는 시각

STARTS_MS = [s * 1000 for s, _ in PHASES]
END_MS = END_S * 1000

HEADERS = {
    "T": "trial_id,session_id,group_id,time_ms,phase,temp_C,material,water_temp_C",
    "P": "trial_id,session_id,group_id,time_ms,phase,piezo",
    "A": "trial_id,session_id,group_id,time_ms,phase,fsr",
    "B": "trial_id,session_id,group_id,time_ms,phase,ax,ay,az,gx,gy,gz",
}
NAMES = {"T": "temp", "P": "piezo", "A": "fsr", "B": "imu"}

st = {"active": False}      # 읽기 스레드와 공유하는 상태
done = threading.Event()


def phase_at(ms):
    return PHASES[bisect.bisect_right(STARTS_MS, ms) - 1][1]


def reader(ser):
    buf = b""
    while True:
        buf += ser.read(ser.in_waiting or 1)
        *lines, buf = buf.split(b"\n")
        for raw in lines:
            handle(raw.decode(errors="ignore").strip())


def handle(line):
    if not st["active"] or line[:2] not in ("P,", "A,", "B,", "T,"):
        return
    f = line.split(",")
    try:
        t_us = int(f[1])
    except (IndexError, ValueError):
        return                                         # 깨진 줄은 버림
    kind = f[0]
    if st["t0"] is None:
        st["t0"] = t_us                                # Enter 후 처음 받은 줄 = time_ms 0
    ms = ((t_us - st["t0"]) & 0xFFFFFFFF) / 1000       # 타임스탬프 넘침 대비
    if ms >= END_MS + 200:                             # 늦게 오는 줄을 0.2초 더 기다린 뒤 종료
        st["active"] = False
        for fh in st["files"].values():
            fh.flush()
        done.set()
        return
    ids = f'{st["trial"]},{st["session"]},{st["group"]}'
    out = st["files"][kind]
    if kind == "P":
        for i, v in enumerate(f[2:]):
            m = ms + i * PIEZO_DT_US / 1000
            if m <= END_MS:
                out.write(f"{ids},{m:.3f},{phase_at(m)},{v}\n")
        return
    if ms > END_MS:
        return
    ph = phase_at(ms)
    if kind == "T":
        out.write(f'{ids},{round(ms)},{ph},{f[2]},{st["material"]},{st["water"]}\n')
        if ph != st["shown"]:                          # 진행 상황 표시 (0.5초 단위)
            st["shown"] = ph
            print(f"    {ms / 1000:6.1f}s  {ph}")
    else:
        out.write(f'{ids},{ms:.3f},{ph},{",".join(f[2:])}\n')


def open_files(session):
    files = {}
    for k, name in NAMES.items():
        path = f"{name}_{session}.csv"
        new = not os.path.exists(path)
        files[k] = open(path, "a")
        if new:
            files[k].write(HEADERS[k] + "\n")
    return files


def last_trial(session):
    path = f"temp_{session}.csv"
    if not os.path.exists(path):
        return 0
    nums = [int(l[1:4]) for l in open(path) if l[:1] == "T" and l[1:4].isdigit()]
    return max(nums, default=0)


def ask_conditions():
    session = input("session_id (예: S01): ").strip()
    group = input("group_id (예: S01_G001): ").strip()
    material = input("material (0 또는 1): ").strip()
    water = input("water_temp_C (기준 온도계 값): ").strip()
    return session, group, material, water


def main():
    ser = serial.Serial(PORT, 115200, timeout=1)
    threading.Thread(target=reader, args=(ser,), daemon=True).start()
    session = None
    trial_no = 0
    while True:
        if session is None:
            if "files" in st:
                for fh in st["files"].values():
                    fh.close()
            session, group, material, water = ask_conditions()
            trial_no = last_trial(session)             # 같은 세션 파일이 있으면 번호 이어감
            st.update(session=session, group=group, material=material, water=water,
                      files=open_files(session))
        k = input(f"\n로봇 시퀀스 시작과 동시에 [Enter] = Trial T{trial_no + 1:03d} 시작 / [n] 조건 변경 / [q] 종료: ").strip().lower()
        if k == "q":
            break
        if k == "n":
            session = None
            continue
        trial_no += 1
        done.clear()
        st.update(trial=f"T{trial_no:03d}", t0=None, shown=None, active=True)
        print(f"  기록 중... {END_S}초 뒤 자동 종료")
        done.wait()
        print(f"  T{trial_no:03d} 저장 완료")
    for fh in st["files"].values():
        fh.close()


if __name__ == "__main__":
    main()
