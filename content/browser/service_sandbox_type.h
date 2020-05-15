// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_
#define CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_

#include "build/build_config.h"
#include "content/public/browser/sandbox_type.h"
#include "content/public/browser/service_process_host.h"

// This file maps service classes to sandbox types.  Services which
// require a non-utility sandbox can be added here.  See
// ServiceProcessHost::Launch() for how these templates are consumed.

// audio::mojom::AudioService
namespace audio {
namespace mojom {
class AudioService;
}
}  // namespace audio
template <>
inline content::SandboxType
content::GetServiceSandboxType<audio::mojom::AudioService>() {
  return content::SandboxType::kAudio;
}

// media::mojom::CdmService
namespace media {
namespace mojom {
class CdmService;
}
}  // namespace media
template <>
inline content::SandboxType
content::GetServiceSandboxType<media::mojom::CdmService>() {
  return content::SandboxType::kCdm;
}

// network::mojom::NetworkService
namespace network {
namespace mojom {
class NetworkService;
}
}  // namespace network
template <>
inline content::SandboxType
content::GetServiceSandboxType<network::mojom::NetworkService>() {
  return content::SandboxType::kNetwork;
}

// device::mojom::XRDeviceService
#if defined(OS_WIN)
namespace device {
namespace mojom {
class XRDeviceService;
}
}  // namespace device
template <>
inline content::SandboxType
content::GetServiceSandboxType<device::mojom::XRDeviceService>() {
  return content::SandboxType::kXrCompositing;
}
#endif  // OS_WIN

// video_capture::mojom::VideoCaptureService
namespace video_capture {
namespace mojom {
class VideoCaptureService;
}
}  // namespace video_capture
template <>
inline content::SandboxType
content::GetServiceSandboxType<video_capture::mojom::VideoCaptureService>() {
  return content::SandboxType::kVideoCapture;
}

#endif  // CONTENT_BROWSER_SERVICE_SANDBOX_TYPE_H_
