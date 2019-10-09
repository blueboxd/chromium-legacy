// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ANDROID_DEV_UI_LOADER_DEV_UI_LOADER_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ANDROID_DEV_UI_LOADER_DEV_UI_LOADER_MESSAGE_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/android/features/dev_ui/buildflags.h"
#include "content/public/browser/web_ui_message_handler.h"

#if !defined(OS_ANDROID) || !BUILDFLAG(DFMIFY_DEV_UI)
#error Unsupported platform.
#endif

namespace base {
class ListValue;
class Value;
}  // namespace base

class DevUiLoaderMessageHandler : public content::WebUIMessageHandler {
 public:
  DevUiLoaderMessageHandler();
  ~DevUiLoaderMessageHandler() override;

 private:
  DevUiLoaderMessageHandler(const DevUiLoaderMessageHandler&) = delete;
  void operator=(const DevUiLoaderMessageHandler&) = delete;

  // WebUIMessageHandler
  void RegisterMessages() override;

  // Called from JavaScript. |args| specifies id for callback, which receives
  // one of the following responses:
  // * "not-installed" if the DevUI DFM is not installed.
  // * "installed" if the DevUI DFM is installed.
  void HandleGetDevUiDfmState(const base::ListValue* args);

  // Helper for HandleInstallAndLoadDevUiDfm().
  void ReplyToJavaScript(const base::Value& callback_id,
                         const char* return_value);

  // Called from JavaScript. |args| specifies id for callback, which receives
  // one of the following responses:
  // * "noop" if the DevUI DFM is already installed and loaded.
  // * "success" if DevUI DFM install takes place, and succeeds.
  // * "failure" if DevUI DFM install takes place, but fails.
  void HandleInstallDevUiDfm(const base::ListValue* args);

  // Callback for dev_ui::DevUiModuleProvider::InstallModule().
  void OnDevUiDfmInstallWithStatus(std::string callback_id_string,
                                   bool success);

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<DevUiLoaderMessageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ANDROID_DEV_UI_LOADER_DEV_UI_LOADER_MESSAGE_HANDLER_H_
