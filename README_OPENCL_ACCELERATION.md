# Android OpenCL image detection build

This branch adds OpenCL acceleration to Smart AutoClicker's existing OpenCV
template-matching path.

## What is accelerated

- Image conditions still use OpenCV `TM_CCOEFF_NORMED` template matching.
- Screen and template grayscale images plus the correlation output use
  `cv::UMat`, which enables OpenCV's OpenCL `matchTemplate` implementation.
- Result validation and candidate color verification remain CPU-side.
- Color-only conditions and NCNN-based OCR are not GPU-accelerated in this phase.

## Android provider discovery

The build uses the Khronos OpenCL headers and ICD loader. The loader is built as
a position-independent static library for `arm64-v8a` and patched with the
Android direct-provider bridge adapted from the successful
[FFmpeg OpenCL CI run](https://github.com/mahfujxamon/ffmpeg-studio-github-ci-opencl/actions/runs/37784343661).

At application startup a small native probe checks device-supplied libraries
for `clGetPlatformIDs` and `clIcdGetPlatformIDsKHR` symbols. It does not call
OpenCL APIs during discovery. Direct Android providers are selected with
`SMART_AUTOCLICKER_OPENCL_DIRECT_LIBRARY`; standard Khronos ICD discovery is
left enabled.

No proprietary vendor GPU library is packaged. The app manifest declares
known vendor library names as optional native libraries so Android can expose
them when the device firmware provides them.

## Build

The `Build OpenCL accelerated APK` workflow:

1. Installs Android NDK 28.2.13676358.
2. Clones pinned Khronos headers and the ICD loader.
3. Patches the Khronos loader for Android direct providers.
4. Builds the static loader for `arm64-v8a`.
5. Builds `assembleFDroidDebug` and runs local unit tests.
6. Uploads the resulting debug APKs as
   `Smart-AutoClicker-OpenCL-arm64-debug`.

The Gradle detection module enables OpenCL only when both
`opencl-headers/CL/cl.h` and `icd-loader/build/libOpenCL.a` exist. Local
builds without those prepared dependencies keep the original CPU backend.

## Runtime verification

A successful CI build verifies compilation and linking, not the availability
of an OpenCL driver on every phone. After installing the APK, inspect logcat
for the `SmartAutoClickerOpenCL` tag and compare detection latency on the
target device. OpenCV UMat operations retain CPU fallback when OpenCL is not
available.
