from __future__ import annotations

import argparse
import glob
import json
import os
import time
from datetime import datetime, timedelta, timezone

from metrics import MetricsCollector
from telemetry_client import TelemetryClient

KST = timezone(timedelta(hours=9))


class SessionAgg:
    def __init__(self, throttle_temp: float = 87.0):
        self.temps, self.powers = [], []
        self.gps_ok, self.gps_n = 0, 0
        self.throttle_temp = throttle_temp

    def update(self, snap: dict) -> None:
        if snap["temp"] is not None:
            self.temps.append(snap["temp"])
        if snap["power"] is not None:
            self.powers.append(snap["power"])
        self.gps_n += 1
        self.gps_ok += int(bool(snap["gps"]))

    def summary(self) -> dict:
        mx = lambda a: max(a) if a else None
        return {
            "max_temp": mx(self.temps),
            "avg_temp": round(sum(self.temps) / len(self.temps), 1) if self.temps else None,
            "throttle_temp": self.throttle_temp,
            "max_power": mx(self.powers),
            "avg_power": round(sum(self.powers) / len(self.powers), 2) if self.powers else None,
            "gps_reception": round(self.gps_ok / self.gps_n, 2) if self.gps_n else None,
        }


def recorded_at_from(fix: dict, fallback: str | None) -> str:
    utc = fix.get("utc")
    if utc:
        try:
            return datetime.fromisoformat(utc).astimezone(KST).isoformat(timespec="seconds")
        except ValueError:
            pass
    return fallback or datetime.now(KST).isoformat(timespec="seconds")


def load_snapshot(meta_path: str):
    with open(meta_path, encoding="utf-8") as f:
        meta = json.load(f)
    image_path = os.path.join(os.path.dirname(meta_path), meta["image"])
    with open(image_path, "rb") as f:
        image = f.read()
    return meta, image


def open_gps(a):
    if a.no_gps:
        return None
    if a.gps_source == "phone":
        from phone_gps import PhoneGPS
        return PhoneGPS(a.phone_host, a.phone_port, a.phone_proto).start()
    from gps import GPS
    return GPS(port=a.gps_port).start()


def open_jtop():
    try:
        from jtop import jtop
        jt = jtop()
        jt.start()
        return jt
    except Exception:
        return None


def run(a) -> None:
    gps = open_gps(a)
    jt = open_jtop()
    collector = MetricsCollector(None, gps, jt)
    agg = SessionAgg()
    tc = TelemetryClient(a.url).start()

    start_dt = datetime.now()
    tc.session_start(a.device, start_dt.strftime("%Y-%m-%d"), start_dt.strftime("%H:%M"))
    print(f"[uploader] 시작 · watch={a.watch_dir} · url={a.url} · "
          f"gps={'off' if gps is None else a.gps_source} · jtop={'on' if jt else 'off'}")

    pattern = os.path.join(a.watch_dir, "*.json")
    seen = set() if a.send_existing else set(glob.glob(pattern))
    sent = 0
    last_tele = 0.0
    try:
        while True:
            for path in sorted(set(glob.glob(pattern)) - seen):
                seen.add(path)
                try:
                    meta, image = load_snapshot(path)
                except (OSError, ValueError, KeyError) as e:
                    print(f"[uploader] 읽기 실패(건너뜀): {os.path.basename(path)} {e}")
                    continue
                fix = gps.read() if gps is not None else {}
                tc.detection(grade=meta["grade"], hazard_type=meta.get("hazard_type"),
                             conf=meta.get("conf"), lat=fix.get("lat"), lon=fix.get("lon"),
                             image_bytes=image,
                             recorded_at=recorded_at_from(fix, meta.get("recorded_at")),
                             detections=meta.get("detections"))
                sent += 1
                print(f"[uploader] 판정 전송 #{sent}: {meta['grade']} conf={meta.get('conf')} "
                      f"gps=({fix.get('lat')},{fix.get('lon')}) 대기={tc.pending()}")

            now = time.time()
            if now - last_tele >= a.interval:
                try:
                    snap = collector.snapshot()
                    agg.update(snap)
                    t = datetime.now().strftime("%H:%M")
                    tc.telemetry(t, snap["temp"], snap["power"], snap["drops"], snap["gps"])
                    print(f"[uploader] telemetry {t} · temp={snap['temp']} power={snap['power']} "
                          f"gps={snap['gps']} · 대기={tc.pending()}")
                except Exception as e:
                    print(f"[uploader] telemetry 오류(계속): {e}")
                last_tele = now

            if a.duration is not None and (time.time() - start_dt.timestamp()) >= a.duration:
                break
            time.sleep(a.poll)
    except KeyboardInterrupt:
        print("\n[uploader] 중단(Ctrl+C)")
    finally:
        end_dt = datetime.now()
        summary = agg.summary()
        tc.session_end(end_time=end_dt.strftime("%H:%M"),
                       duration_sec=int((end_dt - start_dt).total_seconds()), **summary)
        tc.stop()
        if gps is not None:
            gps.stop()
        if jt is not None:
            try:
                jt.close()
            except Exception:
                pass
        print(f"[uploader] 종료 · 판정 전송 {sent}건 · 요약={summary}")


def main() -> None:
    ap = argparse.ArgumentParser(description="HailMary 판정 결과·기기 상태 → 서버 전송")
    ap.add_argument("--url", default="ws://127.0.0.1:8000/ws/device")
    ap.add_argument("--watch-dir", default=os.environ.get("HAILMARY_SNAPSHOT_DIR", "snapshots"),
                    help="C++ Judge가 snapshot jpg/json을 쓰는 폴더")
    ap.add_argument("--interval", type=float, default=60.0, help="telemetry 전송 간격(초)")
    ap.add_argument("--poll", type=float, default=0.3, help="snapshot 폴더 확인 간격(초)")
    ap.add_argument("--duration", type=float, default=None, help="최대 실행(초). 미지정 시 Ctrl+C까지")
    ap.add_argument("--device", default="Jetson Orin Nano")
    ap.add_argument("--send-existing", action="store_true", help="시작 전에 있던 snapshot도 전송")
    ap.add_argument("--no-gps", action="store_true")
    ap.add_argument("--gps-source", default="serial", choices=["serial", "phone"])
    ap.add_argument("--gps-port", default="/dev/ttyACM0")
    ap.add_argument("--phone-host", default="192.168.42.129")
    ap.add_argument("--phone-port", type=int, default=11123)
    ap.add_argument("--phone-proto", default="tcp", choices=["tcp", "udp"])
    run(ap.parse_args())


if __name__ == "__main__":
    main()
