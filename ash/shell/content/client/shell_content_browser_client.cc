// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_content_browser_client.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/shell/content/client/shell_browser_main_parts.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/utility/content_utility_client.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash {
namespace shell {

ShellContentBrowserClient::ShellContentBrowserClient() = default;

ShellContentBrowserClient::~ShellContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
ShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return std::make_unique<ShellBrowserMainParts>(parameters);
}

}  // namespace shell
}  // namespace ash
