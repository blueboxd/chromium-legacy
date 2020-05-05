// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/user_agent.h"

#include "base/command_line.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace weblayer {

std::string GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

std::string GetUserAgent() {
  std::string product = GetProduct();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // On android, content adds this switch automatically if the right conditions
  // are met.
  if (command_line.HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";

  return content::BuildUserAgentFromProduct(product);
}

blink::UserAgentMetadata GetUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  std::string major_version = version_info::GetMajorVersionNumber();
  metadata.brand_version_list.push_back({"Chromium", major_version});

  // The CHROMIUM_BRANDING build flag is not available in //weblayer so we're
  // going to assume it's a derivative.
  metadata.brand_version_list.push_back(
      {version_info::GetProductName(), major_version});

  metadata.full_version = version_info::GetVersionNumber();
  metadata.platform = version_info::GetOSType();
  metadata.architecture = content::BuildCpuInfo();
  metadata.model = content::BuildModelInfo();
  metadata.mobile = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseMobileUserAgent);
  return metadata;
}

}  // namespace weblayer
