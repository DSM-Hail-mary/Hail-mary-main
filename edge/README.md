# edge — 판정 결과·기기 상태 전송 (Python)

C++ 파이프라인(`HailMary/`)이 저장한 판정 결과를 GPS 좌표와 함께 서버로 보내고, 기기 상태(온도·전력·GPS 수신)를 주기적으로 보낸다.
원래 `Hail-mary-Camera` 레포였고, 커밋 기록은 그대로 가져왔다.

```
HailMary(C++) ──snapshots/*.jpg + *.json──▶ uploader.py ──WS /ws/device──▶ Server
                                              ▲
                                     GPS(시리얼 / 폰) · 기기 지표
```

| 파일 | 역할 |
|---|---|
| `uploader.py` | 실행 진입점. snapshot 폴더 감시 → `detection` 전송, 주기적 `telemetry`, 세션 시작/종료 |
| `telemetry_client.py` | 서버 WebSocket 전송. 재연결 + 오프라인 버퍼 |
| `gps.py` / `phone_gps.py` | NMEA 수신 — 시리얼(u-blox) / 네트워크(폰, TCP·UDP) |
| `metrics.py` | 온도(thermal zone) · 전력(jtop·sysfs) · GPS 수신 |

## snapshot 형식 (C++ Judge → uploader)

`snapshot_<시각>_<번호>.jpg`(탐지 영역 크롭, 서버 전송)와 같은 이름의 `.json`. 빨간 박스를 그린 전체 프레임 `snapshot_<시각>_<번호>_box.jpg`는 Jetson에만 저장:

```json
{"grade": "danger", "hazard_type": "nest", "conf": 0.7, "recorded_at": "2026-11-08T19:18:17+09:00",
 "image": "snapshot_20261108_191817_0.jpg",
 "detections": [{"kind": "nest", "score": 0.7, "box": {"x": 0.068, "y": 0.069, "w": 0.864, "h": 0.862}}]}
```

- `grade`: `danger`(conf ≥ 0.6) / `caution`(conf ≥ 0.4)
- `detections`: 크롭 기준 0~1 박스 (까치집 + 겹치는 전주) — 대시보드 상세 화면에 그대로 표시
- 기록 시각은 GPS 시각이 있으면 GPS, 없으면 json의 `recorded_at`

## 실행

```bash
pip install -r requirements.txt

# 1) C++ 파이프라인 (HAILMARY_SHOW=1 이면 탐지 박스 화면 표시)
cd HailMary/build && HAILMARY_SNAPSHOT_DIR=~/hailmary-snapshots HAILMARY_SHOW=1 ./HailMary

# 2) 업로더
python edge/uploader.py --url ws://<서버IP>:8000/ws/device --watch-dir ~/hailmary-snapshots \
    --gps-source phone --phone-host <폰IP>
```

주요 플래그: `--interval`(telemetry 초) · `--gps-source serial|phone` · `--no-gps` · `--send-existing` · `--duration`

## 알려진 한계
- 기기 상태의 fps·프레임 드롭은 아직 전송하지 않음 (캡처가 C++로 옮겨감) — 드롭은 0으로 표시
- GPS: iPhone-WiFi(GPS2IP)는 iOS 절전으로 링크 불안정 — Android USB 테더/GPS 모듈 권장
