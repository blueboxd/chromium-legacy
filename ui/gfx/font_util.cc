// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_util.h"

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <fontconfig/fontconfig.h>
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/direct_write.h"
#endif

namespace gfx {

void InitializeFonts() {
  // Implicit initialization can cause a long delay on the first rendering if
  // the font cache has to be regenerated for some reason. Doing it explicitly
  // here helps in cases where the browser process is starting up in the
  // background (resources have not yet been granted to cast) since it prevents
  // the long delay the user would have seen on first rendering.

#if defined(OS_LINUX)
  // Without this call, the FontConfig library gets implicitly initialized
  // on the first call to FontConfig. Since it's not safe to initialize it
  // concurrently from multiple threads, we explicitly initialize it here
  // to prevent races when there are multiple renderer's querying the library:
  // http://crbug.com/404311
  // Note that future calls to FcInit() are safe no-ops per the FontConfig
  // interface.
  FcInit();
#endif  // OS_LINUX

#if defined(OS_WIN)
  gfx::win::InitializeDirectWrite();
#endif  // OS_WIN
}

}  // namespace gfx
