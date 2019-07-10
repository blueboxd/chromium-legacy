// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_
#define MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_

#include <d3d11_1.h>
#include <wrl/client.h>

#include "ui/gfx/geometry/rect.h"

namespace media {

bool IsLegacyGPU(ID3D11Device* device);

// Returns true if a ID3D11VideoDecoder can be created for |resolution_to_test|
// on the given |video_device|.
bool IsResolutionSupportedForDevice(const gfx::Size& resolution_to_test,
                                    const GUID& decoder_guid,
                                    ID3D11VideoDevice* video_device,
                                    DXGI_FORMAT format);

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_SUPPORTED_PROFILE_HELPERS_H_
