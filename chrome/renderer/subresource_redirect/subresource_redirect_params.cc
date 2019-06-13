// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

namespace subresource_redirect {

bool ShouldForceEnableSubresourceRedirect() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableSubresourceRedirect);
}

std::string LitePageSubresourceHost() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLitePagesServerSubresourceHost)) {
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kLitePagesServerSubresourceHost);
  }
  return "https://litepages.googlezip.net/";
}

}  // namespace subresource_redirect
