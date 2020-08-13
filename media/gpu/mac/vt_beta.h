// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_BETA_H_
#define MEDIA_GPU_MAC_VT_BETA_H_

// Dynamic library loader.
#include "media/gpu/mac/vt_beta_stubs.h"

// CoreMedia and VideoToolbox types.
#include "media/gpu/mac/vt_beta_stubs_header.fragment"

// CoreMedia and VideoToolbox functions.
extern "C" {
#include "media/gpu/mac/vt_beta.sig"
}  // extern "C"

#endif  // MEDIA_GPU_MAC_VT_BETA_H_
