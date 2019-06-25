// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/platform/features.h"

namespace mojo {
namespace features {

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Enables the ChannelMac implementation, which uses Mach IPC as the underlying
// transport mechanism for PlatformChannel. Otherwise, macOS defaults to using
// ChannelPosix.
const base::Feature kMojoChannelMac{"MojoChannelMac",
                                    base::FEATURE_ENABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace mojo
