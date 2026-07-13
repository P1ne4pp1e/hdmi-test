#include "hdmi_test/hik_camera.hpp"

#include <MvCameraControl.h>
#include <PixelType.h>

#include <cstring>
#include <chrono>
#include <sstream>

namespace hdmi_test {
namespace {

std::string sdk_error(const char* operation, int code) {
  std::ostringstream stream;
  stream << operation << " failed: 0x" << std::hex << static_cast<unsigned int>(code);
  return stream.str();
}

std::string sdk_string(const unsigned char* value, std::size_t size) {
  const auto* text = reinterpret_cast<const char*>(value);
  const std::size_t length = strnlen(text, size);
  return {text, length};
}

}  // namespace

HikCamera::HikCamera(HikCameraConfig config) : config_(config) {}

HikCamera::~HikCamera() { close(); }

bool HikCamera::enumerate(std::vector<HikCameraInfo>& devices, std::string& error) {
  devices.clear();
  const int init = MV_CC_Initialize();
  if (init != MV_OK) {
    error = sdk_error("MV_CC_Initialize", init);
    return false;
  }
  MV_CC_DEVICE_INFO_LIST list{};
  const int result = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &list);
  if (result != MV_OK) {
    error = sdk_error("MV_CC_EnumDevices", result);
    MV_CC_Finalize();
    return false;
  }
  for (unsigned int index = 0; index < list.nDeviceNum; ++index) {
    const MV_CC_DEVICE_INFO* info = list.pDeviceInfo[index];
    if (!info || info->nTLayerType != MV_USB_DEVICE) continue;
    const auto& usb = info->SpecialInfo.stUsb3VInfo;
    devices.push_back({sdk_string(usb.chModelName, sizeof(usb.chModelName)),
                       sdk_string(usb.chSerialNumber, sizeof(usb.chSerialNumber)),
                       sdk_string(usb.chUserDefinedName, sizeof(usb.chUserDefinedName))});
  }
  MV_CC_Finalize();
  return true;
}

bool HikCamera::open(std::string& error) {
  if (handle_) return true;
  const int init = MV_CC_Initialize();
  if (init != MV_OK) {
    error = sdk_error("MV_CC_Initialize", init);
    return false;
  }
  sdk_initialized_ = true;
  MV_CC_DEVICE_INFO_LIST list{};
  int result = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &list);
  if (result != MV_OK || config_.device_index < 0 ||
      static_cast<unsigned int>(config_.device_index) >= list.nDeviceNum) {
    error = result == MV_OK ? "no requested HIKROBOT USB camera found" : sdk_error("MV_CC_EnumDevices", result);
    close();
    return false;
  }
  result = MV_CC_CreateHandle(&handle_, list.pDeviceInfo[config_.device_index]);
  if (result != MV_OK) {
    error = sdk_error("MV_CC_CreateHandle", result);
    close();
    return false;
  }
  result = MV_CC_OpenDevice(handle_);
  if (result != MV_OK) {
    error = sdk_error("MV_CC_OpenDevice", result);
    close();
    return false;
  }
  if (!apply_configuration(error)) {
    close();
    return false;
  }
  result = MV_CC_StartGrabbing(handle_);
  if (result != MV_OK) {
    error = sdk_error("MV_CC_StartGrabbing", result);
    close();
    return false;
  }
  return true;
}

bool HikCamera::apply_configuration(std::string& error) {
  // The camera can remain in trigger mode or in a previously persisted frame
  // rate cap.  Both states make a 165 FPS USB3 camera appear to run at a much
  // lower rate even though the host capture loop is asynchronous.
  int result = MV_CC_SetEnumValue(handle_, "TriggerMode", 0);
  if (result != MV_OK) {
    error = sdk_error("set TriggerMode", result);
    return false;
  }
  result = MV_CC_SetBoolValue(handle_, "AcquisitionFrameRateEnable", false);
  if (result != MV_OK) {
    error = sdk_error("disable AcquisitionFrameRateEnable", result);
    return false;
  }
  result = MV_CC_SetImageNodeNum(handle_, 4);
  if (result != MV_OK) {
    error = sdk_error("MV_CC_SetImageNodeNum", result);
    return false;
  }
  result = MV_CC_SetFloatValue(handle_, "ExposureTime", static_cast<float>(config_.exposure_time_us));
  if (result != MV_OK) {
    error = sdk_error("set ExposureTime", result);
    return false;
  }
  result = MV_CC_SetFloatValue(handle_, "Gain", static_cast<float>(config_.gain_db));
  if (result != MV_OK) {
    error = sdk_error("set Gain", result);
    return false;
  }
  result = MV_CC_SetBoolValue(handle_, "ReverseX", config_.flip_horizontal);
  if (result != MV_OK) {
    error = sdk_error("set ReverseX", result);
    return false;
  }
  result = MV_CC_SetBoolValue(handle_, "ReverseY", config_.flip_vertical);
  if (result != MV_OK) error = sdk_error("set ReverseY", result);
  return result == MV_OK;
}

bool HikCamera::grab(HikCameraFrame& frame, std::string& error) {
  if (!handle_) {
    error = "camera is not open";
    return false;
  }
  MV_FRAME_OUT raw{};
  const int result = MV_CC_GetImageBuffer(handle_, &raw, 1000);
  if (result != MV_OK) {
    error = sdk_error("MV_CC_GetImageBuffer", result);
    return false;
  }
  const std::size_t size = static_cast<std::size_t>(raw.stFrameInfo.nWidth) * raw.stFrameInfo.nHeight * 3U;
  frame.bgr.resize(size);
  bool converted = true;
  if (raw.stFrameInfo.enPixelType == PixelType_Gvsp_BGR8_Packed) {
    std::memcpy(frame.bgr.data(), raw.pBufAddr, size);
  } else {
    MV_CC_PIXEL_CONVERT_PARAM convert{};
    convert.nWidth = raw.stFrameInfo.nWidth;
    convert.nHeight = raw.stFrameInfo.nHeight;
    convert.pSrcData = raw.pBufAddr;
    convert.nSrcDataLen = raw.stFrameInfo.nFrameLen;
    convert.enSrcPixelType = raw.stFrameInfo.enPixelType;
    convert.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
    convert.pDstBuffer = frame.bgr.data();
    convert.nDstBufferSize = frame.bgr.size();
    const int conversion = MV_CC_ConvertPixelType(handle_, &convert);
    if (conversion != MV_OK) {
      error = sdk_error("MV_CC_ConvertPixelType", conversion);
      converted = false;
    }
  }
  frame.width = static_cast<int>(raw.stFrameInfo.nWidth);
  frame.height = static_cast<int>(raw.stFrameInfo.nHeight);
  frame.frame_number = raw.stFrameInfo.nFrameNum;
  frame.capture_time_ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count());
  MV_CC_FreeImageBuffer(handle_, &raw);
  return converted;
}

void HikCamera::close() {
  if (handle_) {
    MV_CC_StopGrabbing(handle_);
    MV_CC_CloseDevice(handle_);
    MV_CC_DestroyHandle(handle_);
    handle_ = nullptr;
  }
  if (sdk_initialized_) {
    MV_CC_Finalize();
    sdk_initialized_ = false;
  }
}

bool HikCamera::is_open() const { return handle_ != nullptr; }

}  // namespace hdmi_test
