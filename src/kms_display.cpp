#include "hdmi_test/kms_display.hpp"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace hdmi_test {
namespace {

[[noreturn]] void throw_errno(const std::string& action) {
  throw std::system_error(errno, std::generic_category(), action);
}

bool is_hdmi(const drmModeConnector* connector) {
  return connector->connector_type == DRM_MODE_CONNECTOR_HDMIA ||
         connector->connector_type == DRM_MODE_CONNECTOR_HDMIB;
}

drmModeConnector* find_connector(int fd, const drmModeRes* resources) {
  for (int pass = 0; pass < 2; ++pass) {
    for (int index = 0; index < resources->count_connectors; ++index) {
      drmModeConnector* connector =
          drmModeGetConnector(fd, resources->connectors[index]);
      if (connector == nullptr) {
        continue;
      }
      const bool usable = connector->connection == DRM_MODE_CONNECTED &&
                          connector->count_modes > 0;
      const bool matching_type = pass == 0 ? is_hdmi(connector) : true;
      if (usable && matching_type) {
        return connector;
      }
      drmModeFreeConnector(connector);
    }
  }
  return nullptr;
}

std::uint32_t find_crtc(int fd, const drmModeRes* resources,
                        const drmModeConnector* connector) {
  if (connector->encoder_id != 0) {
    drmModeEncoder* encoder = drmModeGetEncoder(fd, connector->encoder_id);
    if (encoder != nullptr) {
      const std::uint32_t current_crtc = encoder->crtc_id;
      drmModeFreeEncoder(encoder);
      if (current_crtc != 0) {
        return current_crtc;
      }
    }
  }

  for (int encoder_index = 0; encoder_index < connector->count_encoders;
       ++encoder_index) {
    drmModeEncoder* encoder =
        drmModeGetEncoder(fd, connector->encoders[encoder_index]);
    if (encoder == nullptr) {
      continue;
    }
    for (int crtc_index = 0; crtc_index < resources->count_crtcs; ++crtc_index) {
      if ((encoder->possible_crtcs & (1U << static_cast<unsigned>(crtc_index))) !=
          0U) {
        const std::uint32_t crtc = resources->crtcs[crtc_index];
        drmModeFreeEncoder(encoder);
        return crtc;
      }
    }
    drmModeFreeEncoder(encoder);
  }
  return 0;
}

drmModeModeInfo find_mode(const drmModeConnector* connector) {
  for (int index = 0; index < connector->count_modes; ++index) {
    if ((connector->modes[index].type & DRM_MODE_TYPE_PREFERRED) != 0U) {
      return connector->modes[index];
    }
  }
  return connector->modes[0];
}

}  // namespace

std::string find_drm_device() {
  std::string fallback;
  for (int index = 0; index < 16; ++index) {
    const std::string path = "/dev/dri/card" + std::to_string(index);
    if (!std::filesystem::exists(path)) {
      continue;
    }
    if (fallback.empty()) {
      fallback = path;
    }
    const int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      continue;
    }
    drmModeRes* resources = drmModeGetResources(fd);
    const bool has_connector =
        resources != nullptr && resources->count_connectors > 0;
    if (resources != nullptr) {
      drmModeFreeResources(resources);
    }
    close(fd);
    if (has_connector) {
      return path;
    }
  }
  return fallback;
}

std::string describe_drm_devices() {
  std::ostringstream output;
  bool found_device = false;
  for (int index = 0; index < 16; ++index) {
    const std::string path = "/dev/dri/card" + std::to_string(index);
    if (!std::filesystem::exists(path)) {
      continue;
    }
    found_device = true;
    output << path;
    const int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      output << ": cannot open (" << std::strerror(errno) << ")\n";
      continue;
    }
    drmModeRes* resources = drmModeGetResources(fd);
    if (resources == nullptr) {
      output << ": cannot query resources (" << std::strerror(errno) << ")\n";
      close(fd);
      continue;
    }
    output << ": " << resources->count_connectors << " connector(s)\n";
    for (int connector_index = 0;
         connector_index < resources->count_connectors; ++connector_index) {
      drmModeConnector* connector =
          drmModeGetConnector(fd, resources->connectors[connector_index]);
      if (connector == nullptr) {
        continue;
      }
      const char* type = drmModeGetConnectorTypeName(connector->connector_type);
      const char* status = connector->connection == DRM_MODE_CONNECTED
                               ? "connected"
                               : connector->connection == DRM_MODE_DISCONNECTED
                                     ? "disconnected"
                                     : "unknown";
      output << "  - " << (type == nullptr ? "unknown" : type) << '-'
             << connector->connector_type_id << ": " << status << ", "
             << connector->count_modes << " mode(s)\n";
      drmModeFreeConnector(connector);
    }
    drmModeFreeResources(resources);
    close(fd);
  }
  if (!found_device) {
    output << "No DRM device found. Expected /dev/dri/card*.\n";
  }
  return output.str();
}

KmsDisplay::KmsDisplay(std::string device_path)
    : device_path_(std::move(device_path)) {}

KmsDisplay::~KmsDisplay() { release(); }

void KmsDisplay::initialize() {
  try {
    fd_ = open(device_path_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
      throw_errno("cannot open DRM device " + device_path_);
    }

    drmModeRes* resources = drmModeGetResources(fd_);
    if (resources == nullptr) {
      throw_errno("cannot query DRM resources");
    }

    drmModeConnector* connector = find_connector(fd_, resources);
    if (connector == nullptr) {
      drmModeFreeResources(resources);
      throw std::runtime_error("no connected HDMI/display connector with a mode found");
    }

    connector_id_ = connector->connector_id;
    drmModeModeInfo mode = find_mode(connector);
    crtc_id_ = find_crtc(fd_, resources, connector);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    if (crtc_id_ == 0) {
      throw std::runtime_error("no compatible CRTC found for connected display");
    }

    original_crtc_ = drmModeGetCrtc(fd_, crtc_id_);
    if (original_crtc_ == nullptr) {
      throw_errno("cannot save current CRTC state");
    }

    width_ = mode.hdisplay;
    height_ = mode.vdisplay;
    drm_mode_create_dumb create{};
    create.width = width_;
    create.height = height_;
    create.bpp = 32;
    if (drmIoctl(fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
      throw_errno("cannot create DRM dumb buffer");
    }
    dumb_handle_ = create.handle;
    pitch_ = create.pitch;
    map_size_ = create.size;

    if (drmModeAddFB(fd_, width_, height_, 24, 32, pitch_, dumb_handle_,
                     &framebuffer_id_) != 0) {
      throw_errno("cannot create DRM framebuffer");
    }

    drm_mode_map_dumb map{};
    map.handle = dumb_handle_;
    if (drmIoctl(fd_, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0) {
      throw_errno("cannot map DRM dumb buffer");
    }
    mapped_pixels_ = mmap(nullptr, map_size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                          fd_, static_cast<off_t>(map.offset));
    if (mapped_pixels_ == MAP_FAILED) {
      mapped_pixels_ = nullptr;
      throw_errno("cannot mmap DRM dumb buffer");
    }
    std::memset(mapped_pixels_, 0, map_size_);

    if (drmModeSetCrtc(fd_, crtc_id_, framebuffer_id_, 0, 0, &connector_id_, 1,
                       &mode) != 0) {
      throw_errno("cannot set DRM CRTC mode");
    }
  } catch (...) {
    release();
    throw;
  }
}

FrameBufferView KmsDisplay::frame_buffer() const {
  return {static_cast<std::uint32_t*>(mapped_pixels_), static_cast<int>(width_),
          static_cast<int>(height_), static_cast<int>(pitch_ / sizeof(std::uint32_t))};
}

std::string KmsDisplay::mode_label() const {
  return std::to_string(width_) + "x" + std::to_string(height_);
}

void KmsDisplay::release() noexcept {
  if (fd_ >= 0 && original_crtc_ != nullptr) {
    auto* crtc = static_cast<drmModeCrtc*>(original_crtc_);
    drmModeSetCrtc(fd_, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y,
                   &connector_id_, 1, &crtc->mode);
    drmModeFreeCrtc(crtc);
    original_crtc_ = nullptr;
  }
  if (mapped_pixels_ != nullptr) {
    munmap(mapped_pixels_, map_size_);
    mapped_pixels_ = nullptr;
  }
  if (fd_ >= 0 && framebuffer_id_ != 0) {
    drmModeRmFB(fd_, framebuffer_id_);
    framebuffer_id_ = 0;
  }
  if (fd_ >= 0 && dumb_handle_ != 0) {
    drm_mode_destroy_dumb destroy{};
    destroy.handle = dumb_handle_;
    drmIoctl(fd_, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    dumb_handle_ = 0;
  }
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

}  // namespace hdmi_test
