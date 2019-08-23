// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_APP_WEB_CONTENT_MAIN_DELEGATE_H_
#define WEBLAYER_APP_WEB_CONTENT_MAIN_DELEGATE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/app/content_main_delegate.h"
#include "weblayer/public/web_main.h"

namespace weblayer {
class WebContentClient;
class WebContentBrowserClient;

class WebContentMainDelegate : public content::ContentMainDelegate {
 public:
  explicit WebContentMainDelegate(WebMainParams params);
  ~WebContentMainDelegate() override;

  // ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;

 private:
  void InitializeResourceBundle();

  WebMainParams params_;
  std::unique_ptr<WebContentBrowserClient> browser_client_;
  std::unique_ptr<WebContentClient> content_client_;

  DISALLOW_COPY_AND_ASSIGN(WebContentMainDelegate);
};

}  // namespace weblayer

#endif  // WEBLAYER_APP_WEB_CONTENT_MAIN_DELEGATE_H_
