// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_content_browser_client.h"

#include <fuchsia/web/cpp/fidl.h>
#include <string>
#include <utility>

#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"
#include "fuchsia/engine/browser/web_engine_devtools_manager_delegate.h"
#include "fuchsia/engine/common.h"
#include "fuchsia/engine/switches.h"

WebEngineContentBrowserClient::WebEngineContentBrowserClient(
    fidl::InterfaceRequest<fuchsia::web::Context> request)
    : request_(std::move(request)), cdm_service_(&mojo_service_registry_) {}

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

void WebEngineContentBrowserClient::BindInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  mojo_service_registry_.BindInterface(
      interface_name, std::move(interface_pipe), render_frame_host);
}

void WebEngineContentBrowserClient::
    RegisterNonNetworkNavigationURLLoaderFactories(
        int frame_tree_node_id,
        NonNetworkURLLoaderFactoryMap* factories) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kContentDirectories)) {
    (*factories)[kFuchsiaContentDirectoryScheme] =
        std::make_unique<ContentDirectoryLoaderFactory>();
  }
}

void WebEngineContentBrowserClient::
    RegisterNonNetworkSubresourceURLLoaderFactories(
        int render_process_id,
        int render_frame_id,
        NonNetworkURLLoaderFactoryMap* factories) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kContentDirectories)) {
    (*factories)[kFuchsiaContentDirectoryScheme] =
        std::make_unique<ContentDirectoryLoaderFactory>();
  }
}

void WebEngineContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kContentDirectories))
    command_line->AppendSwitch(kContentDirectories);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSoftwareVideoDecoders)) {
    command_line->AppendSwitch(switches::kDisableSoftwareVideoDecoders);
  }
}
