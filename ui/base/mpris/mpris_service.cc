// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mpris/mpris_service.h"

#include "build/branding_buildflags.h"
#include "ui/base/mpris/mpris_service_impl.h"

namespace mpris {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kMprisAPIServiceNamePrefix[] =
    "org.mpris.MediaPlayer2.chrome.instance";
#else
const char kMprisAPIServiceNamePrefix[] =
    "org.mpris.MediaPlayer2.chromium.instance";
#endif
const char kMprisAPIObjectPath[] = "/org/mpris/MediaPlayer2";
const char kMprisAPIInterfaceName[] = "org.mpris.MediaPlayer2";
const char kMprisAPIPlayerInterfaceName[] = "org.mpris.MediaPlayer2.Player";

// static
MprisService* MprisService::GetInstance() {
  return MprisServiceImpl::GetInstance();
}

MprisService::~MprisService() = default;

}  // namespace mpris
