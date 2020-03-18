// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_
#define FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_

#include <lib/fidl/cpp/binding.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/fuchsia/startup_context.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_fuchsia.h"
#include "base/optional.h"
#include "fuchsia/runners/cast/api_bindings_client.h"
#include "fuchsia/runners/cast/application_controller_impl.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"
#include "fuchsia/runners/common/web_component.h"

namespace cr_fuchsia {
class AgentManager;
}

class CastRunner;

FORWARD_DECLARE_TEST(HeadlessCastRunnerIntegrationTest, Headless);

// A specialization of WebComponent which adds Cast-specific services.
class CastComponent : public WebComponent,
                      public fuchsia::web::NavigationEventListener,
                      public base::MessagePumpFuchsia::ZxHandleWatcher {
 public:
  struct CastComponentParams {
    CastComponentParams();
    CastComponentParams(CastComponentParams&&);
    ~CastComponentParams();

    chromium::cast::ApplicationConfigManagerPtr app_config_manager;
    chromium::cast::ApplicationContextPtr application_context;
    std::unique_ptr<base::fuchsia::StartupContext> startup_context;
    std::unique_ptr<cr_fuchsia::AgentManager> agent_manager;
    std::unique_ptr<ApiBindingsClient> api_bindings_client;
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request;
    chromium::cast::ApplicationConfig app_config;
    chromium::cast::UrlRequestRewriteRulesProviderPtr rewrite_rules_provider;
    base::Optional<std::vector<fuchsia::web::UrlRequestRewriteRule>>
        rewrite_rules;
    base::Optional<uint64_t> media_session_id;
  };

  CastComponent(CastRunner* runner, CastComponentParams params);
  ~CastComponent() final;

  // WebComponent overrides.
  void StartComponent() final;

  // Sets a callback that will be invoked when the handle controlling the
  // lifetime of a headless "view" is dropped.
  void set_on_headless_disconnect_for_test(
      base::OnceClosure on_headless_disconnect_cb) {
    on_headless_disconnect_cb_ = std::move(on_headless_disconnect_cb);
  }

  const chromium::cast::ApplicationConfig& application_config() {
    return application_config_;
  }

  cr_fuchsia::AgentManager* agent_manager() { return agent_manager_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(HeadlessCastRunnerIntegrationTest, Headless);

  void OnRewriteRulesReceived(
      std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules);

  // WebComponent overrides.
  void DestroyComponent(int termination_exit_code,
                        fuchsia::sys::TerminationReason reason) final;

  // fuchsia::web::NavigationEventListener implementation.
  // Triggers the injection of API channels into the page content.
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) final;

  // fuchsia::ui::app::ViewProvider implementation.
  void CreateView(
      zx::eventpair view_token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      final;

  // base::MessagePumpFuchsia::ZxHandleWatcher implementation.
  // Called when the headless "view" token is disconnected.
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) final;

  std::unique_ptr<cr_fuchsia::AgentManager> agent_manager_;
  chromium::cast::ApplicationConfig application_config_;
  chromium::cast::UrlRequestRewriteRulesProviderPtr rewrite_rules_provider_;
  std::vector<fuchsia::web::UrlRequestRewriteRule> initial_rewrite_rules_;

  bool constructor_active_ = false;
  std::unique_ptr<NamedMessagePortConnector> connector_;
  std::unique_ptr<ApiBindingsClient> api_bindings_client_;
  std::unique_ptr<ApplicationControllerImpl> application_controller_;
  uint64_t media_session_id_ = 0;
  zx::eventpair headless_view_token_;
  base::MessagePumpForIO::ZxHandleWatchController headless_disconnect_watch_;

  base::OnceClosure on_headless_disconnect_cb_;

  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding_;

  DISALLOW_COPY_AND_ASSIGN(CastComponent);
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_
