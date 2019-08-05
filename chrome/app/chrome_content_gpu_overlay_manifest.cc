// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_content_gpu_overlay_manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_CHROMEOS)
#include "components/arc/mojom/protected_buffer_manager.mojom.h"
#include "components/arc/mojom/video_decode_accelerator.mojom.h"
#include "components/arc/mojom/video_encode_accelerator.mojom.h"
#include "components/arc/mojom/video_protected_buffer_allocator.mojom.h"
#endif  // defined(OS_CHROMEOS)

const service_manager::Manifest& GetChromeContentGpuOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
#if defined(OS_CHROMEOS)
        .ExposeCapability("browser",
                          service_manager::Manifest::InterfaceList<
                              arc::mojom::ProtectedBufferManager,
                              arc::mojom::VideoDecodeAccelerator,
                              arc::mojom::VideoDecodeClient,
                              arc::mojom::VideoEncodeAccelerator,
                              arc::mojom::VideoEncodeClient,
                              arc::mojom::VideoProtectedBufferAllocator>())
#endif  // defined(OS_CHROMEOS)
        .Build()
  };
  return *manifest;
}
