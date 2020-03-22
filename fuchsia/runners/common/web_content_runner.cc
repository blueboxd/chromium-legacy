// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/web_content_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/startup_context.h"
#include "base/logging.h"
#include "fuchsia/runners/buildflags.h"
#include "fuchsia/runners/common/web_component.h"
#include "url/gurl.h"

WebContentRunner::WebContentRunner(
    fuchsia::web::CreateContextParams create_params,
    sys::OutgoingDirectory* outgoing_directory)
    : create_params_(std::move(create_params)),
      is_headless_((create_params_.features() &
                    fuchsia::web::ContextFeatureFlags::HEADLESS) ==
                   fuchsia::web::ContextFeatureFlags::HEADLESS) {
  service_binding_.emplace(outgoing_directory, this);
}

WebContentRunner::WebContentRunner(fuchsia::web::ContextPtr context,
                                   bool is_headless)
    : context_(std::move(context)), is_headless_(is_headless) {}

WebContentRunner::~WebContentRunner() = default;

fuchsia::web::ContextPtr WebContentRunner::CreateWebContext(
    fuchsia::web::CreateContextParams create_params) {
  fuchsia::web::ContextPtr web_context;
  GetContextProvider()->Create(std::move(create_params),
                               web_context.NewRequest());
  web_context.set_error_handler([](zx_status_t status) {
    // If the browser instance died, then exit everything and do not attempt to
    // recover. appmgr will relaunch the runner when it is needed again.
    ZX_LOG(ERROR, status) << "Connection to Context lost.";
  });

  return web_context;
}

fuchsia::web::Context* WebContentRunner::GetContext() {
  if (!context_)
    context_ = CreateWebContext(std::move(create_params_));

  return context_.get();
}

void WebContentRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  GURL url(package.resolved_url);
  if (!url.is_valid()) {
    LOG(ERROR) << "Rejected invalid URL: " << url;
    return;
  }

  std::unique_ptr<WebComponent> component = std::make_unique<WebComponent>(
      this,
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info)),
      std::move(controller_request));
#if BUILDFLAG(WEB_RUNNER_REMOTE_DEBUGGING_PORT) != 0
  component->EnableRemoteDebugging();
#endif
  component->StartComponent();
  component->LoadUrl(url, std::vector<fuchsia::net::http::Header>());
  RegisterComponent(std::move(component));
}

void WebContentRunner::SetWebComponentCreatedCallbackForTest(
    base::RepeatingCallback<void(WebComponent*)> callback) {
  DCHECK(components_.empty());
  web_component_created_callback_for_test_ = std::move(callback);
}

void WebContentRunner::DestroyComponent(WebComponent* component) {
  components_.erase(components_.find(component));
}

void WebContentRunner::RegisterComponent(
    std::unique_ptr<WebComponent> component) {
  if (web_component_created_callback_for_test_)
    web_component_created_callback_for_test_.Run(component.get());

  components_.insert(std::move(component));
}

void WebContentRunner::SetContextProviderForTest(
    fuchsia::web::ContextProviderPtr context_provider) {
  DCHECK(context_provider);
  context_provider_ = std::move(context_provider);
}

fuchsia::web::ContextProvider* WebContentRunner::GetContextProvider() {
  if (!context_provider_) {
    context_provider_ = base::fuchsia::ComponentContextForCurrentProcess()
                            ->svc()
                            ->Connect<fuchsia::web::ContextProvider>();
  }
  return context_provider_.get();
}
