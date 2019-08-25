// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_
#define WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace weblayer {

struct MainParams;

class ContentBrowserClientImpl : public content::ContentBrowserClient {
 public:
  explicit ContentBrowserClientImpl(MainParams* params);
  ~ContentBrowserClientImpl() override;

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

 private:
  MainParams* params_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_CONTENT_BROWSER_CLIENT_IMPL_H_
