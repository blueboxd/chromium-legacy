// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_CONTENT_BROWSER_CLIENT_H_
#define WEBLAYER_BROWSER_WEB_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace weblayer {

class WebBrowserMainParts;
struct WebMainParams;

class WebContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit WebContentBrowserClient(WebMainParams* params);
  ~WebContentBrowserClient() override;

  // ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

  WebBrowserMainParts* shell_browser_main_parts() {
    return shell_browser_main_parts_;
  }

 private:
  WebMainParams* params_;
  // Owned by content::BrowserMainLoop.
  WebBrowserMainParts* shell_browser_main_parts_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_CONTENT_BROWSER_CLIENT_H_
