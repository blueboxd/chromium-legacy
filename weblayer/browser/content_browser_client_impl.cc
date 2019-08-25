// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/content_browser_client_impl.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "content/public/common/user_agent.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "weblayer/browser/browser_main_parts_impl.h"
#include "weblayer/public/main.h"

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/path_utils.h"
#include "weblayer/browser/android_descriptors.h"
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "content/public/common/content_descriptors.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#endif

namespace weblayer {

ContentBrowserClientImpl::ContentBrowserClientImpl(MainParams* params)
    : params_(params) {}

ContentBrowserClientImpl::~ContentBrowserClientImpl() = default;

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClientImpl::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  std::unique_ptr<BrowserMainPartsImpl> browser_main_parts =
      std::make_unique<BrowserMainPartsImpl>(params_, parameters);

  return browser_main_parts;
}

std::string ContentBrowserClientImpl::GetAcceptLangs(
    content::BrowserContext* context) {
  return "en-us,en";
}

content::WebContentsViewDelegate*
ContentBrowserClientImpl::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return nullptr;
}

std::string ContentBrowserClientImpl::GetUserAgent() {
  std::string product = "Chrome/";
  product += params_->full_version;
  return content::BuildUserAgentFromProduct(product);
}

blink::UserAgentMetadata ContentBrowserClientImpl::GetUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand = params_->brand;
  metadata.full_version = params_->full_version;
  metadata.major_version = params_->major_version;
  metadata.platform = content::BuildOSCpuInfo(false);
  metadata.architecture = "";
  metadata.model = "";

  return metadata;
}

#if defined(OS_LINUX) || defined(OS_ANDROID)
void ContentBrowserClientImpl::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  mappings->ShareWithRegion(
      kPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kPakDescriptor));
#endif
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

}  // namespace weblayer
