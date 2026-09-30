#!/bin/bash
set -e
cd "$(dirname "$0")"
if [ ! -f libnvdsinfer_custom_impl_Yolo.so ]; then
  [ -d DeepStream-Yolo ] || git clone https://github.com/marcoslucianops/DeepStream-Yolo
  export CUDA_VER=${CUDA_VER:-12.6}
  make -C DeepStream-Yolo/nvdsinfer_custom_impl_Yolo clean
  make -C DeepStream-Yolo/nvdsinfer_custom_impl_Yolo
  cp DeepStream-Yolo/nvdsinfer_custom_impl_Yolo/libnvdsinfer_custom_impl_Yolo.so .
fi
if [ "${1:-usb}" = "csi" ]; then
  SRC="nvarguscamerasrc ! video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1"
else
  SRC="v4l2src device=/dev/video0 ! video/x-raw,width=1280,height=720 ! videoconvert ! nvvideoconvert ! video/x-raw(memory:NVMM),format=NV12"
fi
gst-launch-1.0 $SRC ! m.sink_0 nvstreammux name=m batch-size=1 width=1280 height=720 ! nvinfer config-file-path=config_infer_primary_yoloV8.txt ! nvvideoconvert ! nvdsosd ! nv3dsink
