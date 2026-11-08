<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/DSM-Hail-mary/.github/main/profile/assets/hailmary-logo-dark.png">
  <img src="https://raw.githubusercontent.com/DSM-Hail-mary/.github/main/profile/assets/hailmary-logo-light.png" alt="HailMary" width="280">
</picture>

# HailMary · Edge Pipeline (C++)

**GStreamer + DeepStream 기반 전주 까치집 탐지 엣지 영상 파이프라인**

![Jetson](https://img.shields.io/badge/Jetson_Orin_Nano-76B900?style=flat-square&logo=nvidia&logoColor=white) ![DeepStream](https://img.shields.io/badge/GStreamer_·_DeepStream-111111?style=flat-square&logo=nvidia&logoColor=white) ![C++](https://img.shields.io/badge/C++14-00599C?style=flat-square&logo=cplusplus&logoColor=white)

<sub>[HailMary 조직](https://github.com/DSM-Hail-mary) · [main](https://github.com/DSM-Hail-mary/Hail-mary-main) · [Camera](https://github.com/DSM-Hail-mary/Hail-mary-Camera) · [Server](https://github.com/DSM-Hail-mary/Hail-mary-Server) · [Front](https://github.com/DSM-Hail-mary/Hail-mary-Front)</sub>

</div>

---

## 역할

HailMary 시스템의 **엣지 영상 처리** 담당입니다. Jetson에서 카메라 프레임을 받아 DeepStream(YOLOv8)으로
까치집·전주를 탐지하고, 문제/정상을 판정해 문제 구간의 스냅샷(검출 박스 표시)을 저장합니다.

## 파이프라인

```
OpenCV(Python) ─TCP─▶ Camera → Caps → AI(nvinfer) → Judge → Sink
```

| 요소 | 역할 |
|---|---|
| Camera | OpenCV 프레임 수신 (tcpserversrc, BGR 1280x720) |
| Caps | 포맷 변환 (RGBA) |
| AI | DeepStream YOLOv8 추론 (crow_house, pole) |
| Judge | 문제/정상 판정, 스냅샷 저장, `JudgeResult` 전달 |
| Sink | 콜백으로 프레임 + 판정 결과 전달 |

## 구조

```
HailMary/
├── main.cpp                     # 진입점 → runPipeline()
├── CMakeLists.txt               # GStreamer + DeepStream 빌드 설정
├── Gstreamer/
│   ├── main_pipeline.*          # 요소 연결, 버스 메시지, 재연결 처리
│   └── Elements/
│       ├── Camera/              # tcpserversrc 입력
│       ├── Caps/                # 포맷 변환
│       ├── AI/                  # nvstreammux + nvinfer, 탐지 결과 meta
│       ├── Judge/               # 판정 + 스냅샷 워커
│       └── Sink/                # 프레임·판정 콜백
└── models/                      # best.onnx, labels.txt, nvinfer 설정
```

## 개발 기록
[관련 IL 보러가기](https://github.com/ilfpns/IL/tree/main/Projects/HailMary)
