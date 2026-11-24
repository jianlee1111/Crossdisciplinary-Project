# cdp_logger.py -- 통합 스케치(stage6_integrated.ino)의 시리얼 출력을 Trial 단위 CSV로 저장
# 실제 보드로는 검증 안 됨 (가짜 시리얼 데이터로만 시험함)
#
# 만드는 파일 (세션마다, 이어쓰기):
#   temp_<session>.csv       trial_id,session_id,group_id,time_ms,phase,temp_C,material,water_temp_C
#   fsr_piezo_<session>.csv  trial_id,session_id,group_id,time_ms,phase,fsr,piezo1,piezo2
#   imu_<session>.csv        trial_id,session_id,group_id,time_ms,phase,ax,ay,az,gx,gy,gz
import os
import threading
import serial

PORT = "COM3"
CONTACT_MS = 15000          # 접촉 후 기록 시간

HEADERS = {
    "T": "trial_id,session_id,group_id,time_ms,phase,temp_C,material,water_temp_C",
    "A": "trial_id,session_id,group_id,time_ms,phase,fsr,piezo1,piezo2",
    "B": "trial_id,session_id,group_id,time_ms,phase,ax,ay,az,gx,gy,gz",
}
NAMES = {"T": "temp", "A": "fsr_piezo", "B": "imu"}

st = {"active": False, "contact_req": False}   # 읽기 스레드와 공유하는 상태
done = threading.Event()


def reader(ser):
    while True:
        line = ser.readline().decode(errors="ignore").strip()
        if not st["active"] or line[:2] not in ("A,", "B,", "T,"):
            continue
        f = line.split(",")
        try:
            t_us = int(f[1])
        except (IndexError, ValueError):
            continue                                   # 깨진 줄은 버림
        kind = f[0]
        if kind == "T":
            if st["t0"] is None:
                st["t0"] = t_us                        # 첫 온도 측정 = time_ms 0
            if st["contact_req"] and st["phase"] == "baseline":
                st["phase"], st["c0"] = "contact", t_us
        if st["t0"] is None:
            continue
        ms = ((t_us - st["t0"]) & 0xFFFFFFFF) / 1000   # micros() 넘침 대비
        ids = f'{st["trial"]},{st["session"]},{st["group"]}'
        if kind == "T":
            row = f'{ids},{round(ms)},{st["phase"]},{f[2]},{st["material"]},{st["water"]}'
        else:
            row = f'{ids},{ms:.1f},{st["phase"]},{",".join(f[2:])}'
        st["files"][kind].write(row + "\n")
        if kind == "T" and st["phase"] == "contact" and \
                ((t_us - st["c0"]) & 0xFFFFFFFF) / 1000 >= CONTACT_MS - 50:
            st["active"] = False
            for fh in st["files"].values():
                fh.flush()
            done.set()


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
        k = input(f"\n[Enter] Trial T{trial_no + 1:03d} 시작 / [n] 조건 변경 / [q] 종료: ").strip().lower()
        if k == "q":
            break
        if k == "n":
            session = None
            continue
        trial_no += 1
        done.clear()
        st.update(trial=f"T{trial_no:03d}", phase="baseline", t0=None, c0=None,
                  contact_req=False, active=True)
        input("  baseline 기록 중... 컵에 대는 순간 [Enter]: ")
        st["contact_req"] = True
        print(f"  contact 기록 중... {CONTACT_MS // 1000}초 뒤 자동 종료")
        done.wait()
        print(f"  T{trial_no:03d} 저장 완료")
    for fh in st["files"].values():
        fh.close()


if __name__ == "__main__":
    main()
