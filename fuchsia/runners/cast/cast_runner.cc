// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <memory>
#include <string>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/startup_context.h"
#include "base/logging.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/runners/cast/cast_component.h"
#include "url/gurl.h"

CastRunner::CastRunner(base::fuchsia::ServiceDirectory* service_directory,
                       fuchsia::web::ContextPtr context)
    : WebContentRunner(service_directory, std::move(context)) {}

CastRunner::~CastRunner() = default;

struct CastRunner::PendingComponent {
  chromium::cast::ApplicationConfigManagerPtr app_config_manager;
  std::unique_ptr<base::fuchsia::StartupContext> startup_context;
  std::unique_ptr<cr_fuchsia::AgentManager> agent_manager;
  std::unique_ptr<ApiBindingsClient> bindings_manager;
  fidl::InterfaceRequest<fuchsia::sys::ComponentController> controller_request;
  chromium::cast::ApplicationConfig app_config;
  fuchsia::web::AdditionalHeadersProviderPtr headers_provider;
  base::Optional<std::vector<fuchsia::net::http::Header>> headers;
};

void CastRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  // Verify that |package| specifies a Cast URI, and pull the app-Id from it.
  constexpr char kCastPresentationUrlScheme[] = "cast";
  constexpr char kCastSecurePresentationUrlScheme[] = "casts";

  GURL cast_url(package.resolved_url);
  if (!cast_url.is_valid() ||
      (!cast_url.SchemeIs(kCastPresentationUrlScheme) &&
       !cast_url.SchemeIs(kCastSecurePresentationUrlScheme)) ||
      cast_url.GetContent().empty()) {
    LOG(ERROR) << "Rejected invalid URL: " << cast_url;
    return;
  }

  // The application configuration asynchronously via the per-component
  // ApplicationConfigManager, the pointer to that service must be kept live
  // until the request completes, or CastRunner is deleted.
  auto pending_component = std::make_unique<PendingComponent>();
  pending_component->startup_context =
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info));
  pending_component->agent_manager = std::make_unique<cr_fuchsia::AgentManager>(
      pending_component->startup_context->incoming_services());
  pending_component->controller_request = std::move(controller_request);

  // Request the configuration for the specified application.
  pending_component->agent_manager->ConnectToAgentService(
      kAgentComponentUrl, pending_component->app_config_manager.NewRequest());
  pending_component->app_config_manager.set_error_handler(
      [this, pending_component = pending_component.get()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "ApplicationConfigManager disconnected.";
        GetConfigCallback(pending_component,
                          chromium::cast::ApplicationConfig());
      });

  // Get binding details from the Agent.
  fidl::InterfaceHandle<chromium::cast::ApiBindings> api_bindings_client;
  pending_component->agent_manager->ConnectToAgentService(
      kAgentComponentUrl, api_bindings_client.NewRequest());
  pending_component->bindings_manager = std::make_unique<ApiBindingsClient>(
      std::move(api_bindings_client),
      base::BindOnce(&CastRunner::MaybeStartComponent, base::Unretained(this),
                     base::Unretained(pending_component.get())));

  // Get AdditionalHeadersProvider from the Agent.
  fidl::InterfaceHandle<fuchsia::web::AdditionalHeadersProvider>
      additional_headers_provider;
  pending_component->agent_manager->ConnectToAgentService(
      kAgentComponentUrl, additional_headers_provider.NewRequest());
  pending_component->headers_provider = additional_headers_provider.Bind();
  pending_component->headers_provider.set_error_handler(
      [this, pending_component = pending_component.get()](zx_status_t error) {
        if (pending_component->headers.has_value())
          return;
        pending_component->headers = {};
        MaybeStartComponent(pending_component);
      });
  pending_component->headers_provider->GetHeaders(
      [this, pending_component = pending_component.get()](
          std::vector<fuchsia::net::http::Header> headers, zx_time_t expiry) {
        pending_component->headers =
            base::Optional<std::vector<fuchsia::net::http::Header>>(
                std::move(headers));
        MaybeStartComponent(pending_component);
      });

  const std::string cast_app_id(cast_url.GetContent());
  pending_component->app_config_manager->GetConfig(
      cast_app_id, [this, pending_component = pending_component.get()](
                       chromium::cast::ApplicationConfig app_config) {
        GetConfigCallback(pending_component, std::move(app_config));
      });

  pending_components_.emplace(std::move(pending_component));
}

const char CastRunner::kAgentComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/cast_agent#meta/cast_agent.cmx";

void CastRunner::GetConfigCallback(
    PendingComponent* pending_component,
    chromium::cast::ApplicationConfig app_config) {
  // Ideally the PendingComponent would be move()d out of |pending_components_|
  // here, but that requires extract(), which isn't available until C++17.
  // Instead find |pending_component| and move() the individual fields out
  // before erase()ing it.
  auto it = pending_components_.find(pending_component);
  DCHECK(it != pending_components_.end());

  // If no configuration was returned then ignore the request.
  if (!app_config.has_web_url()) {
    pending_components_.erase(it);
    DLOG(WARNING) << "No ApplicationConfig was found.";
    return;
  }

  pending_component->app_config = std::move(app_config);

  MaybeStartComponent(pending_component);
}

void CastRunner::MaybeStartComponent(PendingComponent* pending_component) {
  if (pending_component->app_config.IsEmpty())
    return;
  if (!pending_component->bindings_manager->HasBindings())
    return;
  if (!pending_component->headers.has_value())
    return;

  // Create a component based on the returned configuration, and pass it the
  // fields stashed in PendingComponent.
  GURL cast_app_url(pending_component->app_config.web_url());
  auto component = std::make_unique<CastComponent>(
      this, std::move(pending_component->app_config),
      std::move(pending_component->bindings_manager),
      std::move(pending_component->startup_context),
      std::move(pending_component->controller_request),
      std::move(pending_component->agent_manager));
  std::vector<fuchsia::net::http::Header> additional_headers =
      pending_component->headers.value();
  pending_components_.erase(pending_component);

  component->LoadUrl(std::move(cast_app_url), std::move(additional_headers));
  RegisterComponent(std::move(component));
}
