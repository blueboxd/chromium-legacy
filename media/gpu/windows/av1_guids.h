// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_AV1_GUIDS_H_
#define MEDIA_GPU_WINDOWS_AV1_GUIDS_H_

#include <dxva.h>
#include <initguid.h>

#if WDK_NTDDI_VERSION <= NTDDI_WIN10_19H1
DEFINE_GUID(DXVA_ModeAV1_VLD_Profile0,
            0xb8be4ccb,
            0xcf53,
            0x46ba,
            0x8d,
            0x59,
            0xd6,
            0xb8,
            0xa6,
            0xda,
            0x5d,
            0x2a);

DEFINE_GUID(DXVA_ModeAV1_VLD_Profile1,
            0x6936ff0f,
            0x45b1,
            0x4163,
            0x9c,
            0xc1,
            0x64,
            0x6e,
            0xf6,
            0x94,
            0x61,
            0x08);

DEFINE_GUID(DXVA_ModeAV1_VLD_Profile2,
            0x0c5f2aa1,
            0xe541,
            0x4089,
            0xbb,
            0x7b,
            0x98,
            0x11,
            0x0a,
            0x19,
            0xd7,
            0xc8);
#endif  // WDK_NTDDI_VERSION <= NTDDI_WIN10_19H1

#endif  // MEDIA_GPU_WINDOWS_AV1_GUIDS_H_
