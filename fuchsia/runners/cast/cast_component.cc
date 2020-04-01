// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_component.h"

#include <lib/fidl/cpp/binding.h>
#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/common/web_component.h"

namespace {

constexpr int kBindingsFailureExitCode = 129;
constexpr int kRewriteRulesProviderDisconnectExitCode = 130;

}  // namespace

CastComponent::CastComponentParams::CastComponentParams() = default;
CastComponent::CastComponentParams::CastComponentParams(CastComponentParams&&) =
    default;
CastComponent::CastComponentParams::~CastComponentParams() = default;

CastComponent::CastComponent(CastRunner* runner,
                             CastComponent::CastComponentParams params)
    : WebComponent(runner,
                   std::move(params.startup_context),
                   std::move(params.controller_request)),
      agent_manager_(std::move(params.agent_manager)),
      application_config_(std::move(params.app_config)),
      rewrite_rules_provider_(std::move(params.rewrite_rules_provider)),
      initial_rewrite_rules_(std::move(params.rewrite_rules.value())),
      api_bindings_client_(std::move(params.api_bindings_client)),
      media_session_id_(params.media_session_id.value()),
      headless_disconnect_watch_(FROM_HERE),
      navigation_listener_binding_(this) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);
}

CastComponent::~CastComponent() = default;

void CastComponent::StartComponent() {
  if (application_config_.has_enable_remote_debugging() &&
      application_config_.enable_remote_debugging()) {
    WebComponent::EnableRemoteDebugging();
  }

  WebComponent::StartComponent();

  connector_ = std::make_unique<NamedMessagePortConnector>(frame());

  rewrite_rules_provider_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_OK, status)
        << "UrlRequestRewriteRulesProvider disconnected.";
    DestroyComponent(kRewriteRulesProviderDisconnectExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR);
  });
  OnRewriteRulesReceived(std::move(initial_rewrite_rules_));

  frame()->SetMediaSessionId(media_session_id_);
  frame()->ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                               fuchsia::web::AllowInputState::DENY);
  frame()->SetNavigationEventListener(
      navigation_listener_binding_.NewBinding());
  api_bindings_client_->AttachToFrame(
      frame(), connector_.get(),
      base::BindOnce(&CastComponent::DestroyComponent, base::Unretained(this),
                     kBindingsFailureExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR));

  if (application_config_.has_force_content_dimensions()) {
    frame()->ForceContentDimensions(std::make_unique<fuchsia::ui::gfx::vec2>(
        application_config_.force_content_dimensions()));
  }

  application_controller_ = std::make_unique<ApplicationControllerImpl>(
      frame(),
      agent_manager_->ConnectToAgentService<chromium::cast::ApplicationContext>(
          CastRunner::kAgentComponentUrl));

  // Pass application permissions to the frame.
  std::string origin = GURL(application_config_.web_url()).GetOrigin().spec();
  if (application_config_.has_permissions()) {
    for (auto& permission : application_config_.permissions()) {
      fuchsia::web::PermissionDescriptor permission_clone;
      zx_status_t status = permission.Clone(&permission_clone);
      ZX_DCHECK(status == ZX_OK, status);
      frame()->SetPermissionState(std::move(permission_clone), origin,
                                  fuchsia::web::PermissionState::GRANTED);
    }
  }
}

void CastComponent::DestroyComponent(int termination_exit_code,
                                     fuchsia::sys::TerminationReason reason) {
  DCHECK(!constructor_active_);

  WebComponent::DestroyComponent(termination_exit_code, reason);
}

void CastComponent::OnRewriteRulesReceived(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
  frame()->SetUrlRequestRewriteRules(std::move(rewrite_rules), [this]() {
    rewrite_rules_provider_->GetUrlRequestRewriteRules(
        fit::bind_member(this, &CastComponent::OnRewriteRulesReceived));
  });
}

void CastComponent::OnNavigationStateChanged(
    fuchsia::web::NavigationState change,
    OnNavigationStateChangedCallback callback) {
  if (change.has_is_main_document_loaded() && change.is_main_document_loaded())
    connector_->OnPageLoad();
  callback();
}

void CastComponent::CreateView(
    zx::eventpair view_token,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services) {
  if (runner()->is_headless()) {
    // For headless CastComponents, |view_token| does not actually connect to a
    // Scenic View. It is merely used as a conduit for propagating termination
    // signals.
    headless_view_token_ = std::move(view_token);
    base::MessageLoopCurrentForIO::Get()->WatchZxHandle(
        headless_view_token_.get(), false /* persistent */,
        ZX_SOCKET_PEER_CLOSED, &headless_disconnect_watch_, this);

    frame()->EnableHeadlessRendering();
    return;
  }

  WebComponent::CreateView(std::move(view_token), std::move(incoming_services),
                           std::move(outgoing_services));
}

void CastComponent::OnZxHandleSignalled(zx_handle_t handle,
                                        zx_signals_t signals) {
  DCHECK_EQ(signals, ZX_SOCKET_PEER_CLOSED);
  DCHECK(runner()->is_headless());

  frame()->DisableHeadlessRendering();

  if (on_headless_disconnect_cb_)
    std::move(on_headless_disconnect_cb_).Run();
}
