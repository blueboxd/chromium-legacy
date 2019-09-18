// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_component.h"

#include <lib/fidl/cpp/binding.h>
#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_platform_bindings_ids.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/common/web_component.h"

namespace {

constexpr int kBindingsFailureExitCode = 129;
constexpr int kRewriteRulesProviderDisconnectExitCode = 130;

constexpr char kStubBindingsPath[] =
    FILE_PATH_LITERAL("fuchsia/runners/cast/not_implemented_api_bindings.js");

TouchInputPolicy TouchInputPolicyFromApplicationConfig(
    const chromium::cast::ApplicationConfig& application_config) {
  if (!application_config.has_touch_enabled_policy())
    return TouchInputPolicy::UNSPECIFIED;

  if (application_config.touch_enabled_policy())
    return TouchInputPolicy::FORCE_ENABLE;

  return TouchInputPolicy::FORCE_DISABLE;
}

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
      touch_input_policy_(
          TouchInputPolicyFromApplicationConfig(application_config_)),
      connector_(frame()),
      api_bindings_client_(std::move(params.api_bindings_client)),
      navigation_listener_binding_(this) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);

  rewrite_rules_provider_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "UrlRequestRewriteRulesProvider disconnected.";
    DestroyComponent(kRewriteRulesProviderDisconnectExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR);
  });

  DCHECK(params.rewrite_rules);
  OnRewriteRulesReceived(std::move(params.rewrite_rules.value()));

  frame()->SetEnableInput(false);
  frame()->SetNavigationEventListener(
      navigation_listener_binding_.NewBinding());
  api_bindings_client_->AttachToFrame(
      frame(), &connector_,
      base::BindOnce(&CastComponent::DestroyComponent, base::Unretained(this),
                     kBindingsFailureExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR));

  application_controller_ = std::make_unique<ApplicationControllerImpl>(
      frame(), agent_manager_->ConnectToAgentService<
                   chromium::cast::ApplicationControllerReceiver>(
                   CastRunner::kAgentComponentUrl));

  InitializeCastPlatformBindings();
}

CastComponent::~CastComponent() = default;

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
    connector_.OnPageLoad();
  callback();
}

void CastComponent::InitializeCastPlatformBindings() {
  base::FilePath stub_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &stub_path));
  stub_path = stub_path.AppendASCII(kStubBindingsPath);
  DCHECK(base::PathExists(stub_path));
  fuchsia::mem::Buffer stub_buf = cr_fuchsia::MemBufferFromFile(
      base::File(stub_path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  CHECK(stub_buf.vmo);
  frame()->AddBeforeLoadJavaScript(
      static_cast<uint64_t>(CastPlatformBindingsId::NOT_IMPLEMENTED_API), {"*"},
      std::move(stub_buf),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response()) << "Couldn't inject stub bindings.";
      });

  touch_input_ = std::make_unique<TouchInputBindings>(touch_input_policy_,
                                                      frame(), &connector_);
}
