// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <va/va.h>
#include <va/va_drm.h>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/vaapi_device.h"

namespace media {
namespace vaapi_test {

// Returns the preferred VA_RT_FORMAT for the given |profile|.
unsigned int GetFormatForProfile(VAProfile profile) {
  if (profile == VAProfileVP9Profile2)
    return VA_RT_FORMAT_YUV420_10BPP;
  return VA_RT_FORMAT_YUV420;
}

VaapiDevice::VaapiDevice(VAProfile profile)
    : display_(nullptr),
      config_id_(VA_INVALID_ID),
      profile_(profile),
      internal_va_format_(GetFormatForProfile(profile_)) {
  Initialize();
}

VaapiDevice::~VaapiDevice() {
  VLOG(1) << "Tearing down...";
  VAStatus res;
  if (config_id_ != VA_INVALID_ID) {
    res = vaDestroyConfig(display_, config_id_);
    VA_LOG_ASSERT(res, "vaDestroyConfig");
  }
  if (display_ != nullptr) {
    res = vaTerminate(display_);
    VA_LOG_ASSERT(res, "vaTerminate");
  }

  VLOG(1) << "Teardown done.";
}

void VaapiDevice::Initialize() {
  constexpr char kDriRenderNode0Path[] = "/dev/dri/renderD128";
  display_file_ = base::File(
      base::FilePath::FromUTF8Unsafe(kDriRenderNode0Path),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  LOG_ASSERT(display_file_.IsValid())
      << "Failed to open " << kDriRenderNode0Path;

  display_ = vaGetDisplayDRM(display_file_.GetPlatformFile());
  LOG_ASSERT(display_ != nullptr) << "vaGetDisplayDRM failed";

  int major, minor;
  VAStatus res = vaInitialize(display_, &major, &minor);
  VA_LOG_ASSERT(res, "vaInitialize");
  VLOG(1) << "VA major version: " << major << ", minor version: " << minor;

  // Create config.
  // We rely on vaCreateConfig to specify the error mode if decode is not
  // supported for the given profile.
  // TODO(jchinlee): Refactor configuration management to be owned by decoders
  // (this will also allow decoders to adjust the VAConfig as needed, e.g. if
  // the profile changes part-way).
  std::vector<VAConfigAttrib> attribs;
  attribs.push_back({VAConfigAttribRTFormat, internal_va_format_});
  res = vaCreateConfig(display_, profile_, VAEntrypointVLD, attribs.data(),
                       attribs.size(), &config_id_);
  VA_LOG_ASSERT(res, "vaCreateConfig");
}

}  // namespace vaapi_test
}  // namespace media
