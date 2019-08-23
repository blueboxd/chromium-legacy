// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/public/web_main.h"

#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "weblayer/app/web_content_main_delegate.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

namespace weblayer {

WebMainParams::WebMainParams() = default;
WebMainParams::WebMainParams(const WebMainParams& other) = default;
WebMainParams::~WebMainParams() = default;

int WebMain(WebMainParams params,
#if defined(OS_WIN)
#if !defined(WIN_CONSOLE_APP)
            HINSTANCE instance
#endif
#else
            int argc,
            const char** argv
#endif
) {
  WebContentMainDelegate delegate(std::move(params));
  content::ContentMainParams content_params(&delegate);

#if defined(OS_WIN)
#if defined(WIN_CONSOLE_APP)
  HINSTANCE instance = GetModuleHandle(nullptr);
#endif
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  content_params.instance = instance;
  content_params.sandbox_info = &sandbox_info;
#else
  content_params.argc = argc;
  content_params.argv = argv;
#endif

  return content::ContentMain(content_params);
}

}  // namespace weblayer
