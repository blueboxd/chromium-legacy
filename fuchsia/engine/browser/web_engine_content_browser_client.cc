// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_content_browser_client.h"

#include <utility>

#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"
#include "fuchsia/engine/browser/web_engine_devtools_manager_delegate.h"
#include "fuchsia/engine/common.h"

WebEngineContentBrowserClient::WebEngineContentBrowserClient(
    fidl::InterfaceRequest<fuchsia::web::Context> request)
    : request_(std::move(request)) {}

WebEngineContentBrowserClient::~WebEngineContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
WebEngineContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  DCHECK(request_);
  auto browser_main_parts = std::make_unique<WebEngineBrowserMainParts>(
      parameters, std::move(request_));

  main_parts_ = browser_main_parts.get();

  return browser_main_parts;
}

content::DevToolsManagerDelegate*
WebEngineContentBrowserClient::GetDevToolsManagerDelegate() {
  DCHECK(main_parts_);
  DCHECK(main_parts_->browser_context());
  return new WebEngineDevToolsManagerDelegate(main_parts_->browser_context());
}

std::string WebEngineContentBrowserClient::GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

std::string WebEngineContentBrowserClient::GetUserAgent() {
  std::string user_agent = content::BuildUserAgentFromProduct(GetProduct());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kUserAgentProductAndVersion)) {
    user_agent +=
        " " + base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
                  kUserAgentProductAndVersion);
  }
  return user_agent;
}

void WebEngineContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    content::WebPreferences* web_prefs) {
  // Disable WebSQL support since it's being removed from the web platform.
  web_prefs->databases_enabled = false;
}
