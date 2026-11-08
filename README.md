<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/DSM-Hail-mary/.github/main/profile/assets/hailmary-logo-dark.png">
  <img src="https://raw.githubusercontent.com/DSM-Hail-mary/.github/main/profile/assets/hailmary-logo-light.png" alt="HailMary" width="280">
</picture>

# HailMary · Edge Pipeline (C++)

**GStreamer + DeepStream 기반 전주 까치집 탐지 엣지 (탐지·판정·서버 전송)**

![Jetson](https://img.shields.io/badge/Jetson_Orin_Nano-76B900?style=flat-square&logo=nvidia&logoColor=white) ![DeepStream](https://img.shields.io/badge/GStreamer_·_DeepStream-111111?style=flat-square&logo=nvidia&logoColor=white) ![C++](https://img.shields.io/badge/C++14-00599C?style=flat-square&logo=cplusplus&logoColor=white) ![Python](https://img.shields.io/badge/Python-3776AB?style=flat-square&logo=python&logoColor=white)

<sub>[HailMary 조직](https://github.com/DSM-Hail-mary) · [main](https://github.com/DSM-Hail-mary/Hail-mary-main) · [Server](https://github.com/DSM-Hail-mary/Hail-mary-Server) · [Front](https://github.com/DSM-Hail-mary/Hail-mary-Front)</sub>

</div>

---

## 역할

HailMary 시스템의 **차량 탑재 엣지** 전체를 담당합니다. Jetson에서 USB 카메라 영상을 DeepStream(YOLOv8)으로
까치집·전주를 탐지하고 위험/주의를 판정해 크롭 사진과 판정 정보를 저장하면, `edge/uploader.py`가 GPS 좌표·기기 상태와 함께 서버로 전송합니다.

## 파이프라인

```
USB 카메라 → Camera → Caps → AI(nvinfer) → Judge → Sink
                                              │ snapshots/*.jpg + *.json
                                              ▼
                                 edge/uploader.py ─WS─▶ Server
```

| 요소 | 역할 |
|---|---|
| Camera | USB 카메라 MJPG 1280x720@30 캡처 + NVDEC 디코드 (`HAILMARY_CAMERA`, 기본 `/dev/video0`) |
| Caps | 포맷 변환 (RGBA) |
| AI | DeepStream YOLOv8 추론 (crow_house, pole). `HAILMARY_SHOW=1`이면 nvdsosd로 탐지 박스 화면 표시 |
| Judge | 위험/주의 판정, 크롭 jpg + 판정 json 저장 (`HAILMARY_SNAPSHOT_DIR`), `JudgeResult` 전달 |
| Sink | 콜백으로 프레임 + 판정 결과 전달 |

## 구조

```
HailMary/
├── main.cpp                     # 진입점 → runPipeline()
├── CMakeLists.txt               # GStreamer + DeepStream 빌드 설정
├── Gstreamer/
│   ├── main_pipeline.*          # 요소 연결, 버스 메시지, 재연결 처리
│   └── Elements/
│       ├── Camera/              # v4l2src + NVDEC 입력
│       ├── Caps/                # 포맷 변환
│       ├── AI/                  # nvstreammux + nvinfer, 탐지 결과 meta
│       ├── Judge/               # 판정 + 스냅샷 워커
│       └── Sink/                # 프레임·판정 콜백
└── models/                      # best.onnx, labels.txt, nvinfer 설정

edge/                            # 판정 결과·기기 상태 → 서버 전송 (Python, 구 Hail-mary-Camera)
```

## 개발 기록
[관련 IL 보러가기](https://github.com/ilfpns/IL/tree/main/Projects/HailMary)
