// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ANDROID_DEV_UI_LOADER_DEV_UI_LOADER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ANDROID_DEV_UI_LOADER_DEV_UI_LOADER_UI_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/android/features/dev_ui/buildflags.h"
#include "content/public/browser/web_ui_controller.h"

#if !defined(OS_ANDROID) || !BUILDFLAG(DFMIFY_DEV_UI)
#error Unsupported platform.
#endif

class GURL;

class DevUiLoaderUI : public content::WebUIController {
 public:
  DevUiLoaderUI(content::WebUI* web_ui_in, const GURL& url);
  ~DevUiLoaderUI() override;

 private:
  DevUiLoaderUI(const DevUiLoaderUI&) = delete;
  void operator=(const DevUiLoaderUI&) = delete;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<DevUiLoaderUI> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ANDROID_DEV_UI_LOADER_DEV_UI_LOADER_UI_H_
