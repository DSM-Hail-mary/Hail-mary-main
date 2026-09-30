전주 까치집 탐지를 위한 Jetson 엣지 영상 파이프라인 (GStreamer + DeepStream)

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

## 실행 (Jetson)
```bash
cd HailMary/models && CUDA_VER=<CUDA 버전> ./run_deepstream.sh
cd .. && mkdir -p build && cd build && cmake .. && make -j$(nproc)
./HailMary
```

## 개발 기록
[관련 IL 보러가기](https://github.com/ilfpns/IL/tree/main/Projects/HailMary)
