// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "fuchsia/base/agent_manager.h"
#include "url/gurl.h"

namespace {

// List of services provided to the WebEngine context.
// All services must be listed in cast_runner.cmx.
static constexpr const char* kServices[] = {
    "fuchsia.accessibility.semantics.SemanticsManager",
    "fuchsia.device.NameProvider",
    "fuchsia.fonts.Provider",
    "fuchsia.intl.PropertyProvider",
    "fuchsia.logger.LogSink",
    "fuchsia.media.SessionAudioConsumerFactory",
    "fuchsia.media.drm.PlayReady",
    "fuchsia.media.drm.Widevine",
    "fuchsia.mediacodec.CodecFactory",
    "fuchsia.memorypressure.Provider",
    "fuchsia.net.NameLookup",
    "fuchsia.netstack.Netstack",
    "fuchsia.posix.socket.Provider",
    "fuchsia.process.Launcher",
    "fuchsia.sysmem.Allocator",
    "fuchsia.ui.input.ImeService",
    "fuchsia.ui.input.ImeVisibilityService",
    "fuchsia.ui.scenic.Scenic",
    "fuchsia.vulkan.loader.Loader",

    // These services are redirected to the Agent:
    // * fuchsia.media.Audio
    // * fuchsia.legacymetrics.MetricsRecorder
};

// Creates a CreateContextParams object which can be used as a basis
// for starting isolated Runners.
fuchsia::web::CreateContextParams BuildCreateContextParamsForIsolatedRunners(
    const fuchsia::web::CreateContextParams& create_context_params) {
  fuchsia::web::CreateContextParams output;

  // Isolated contexts are only allowed a limited set of features.
  // Only pass those features from |create_context_params|.
  static constexpr fuchsia::web::ContextFeatureFlags kAllowedFeatures =
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS |
      fuchsia::web::ContextFeatureFlags::HEADLESS |
      fuchsia::web::ContextFeatureFlags::VULKAN |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;
  DCHECK(create_context_params.has_features());
  output.set_features(create_context_params.features() & kAllowedFeatures);

  if (create_context_params.has_user_agent_product()) {
    output.set_user_agent_product(create_context_params.user_agent_product());
  }
  if (create_context_params.has_user_agent_version()) {
    output.set_user_agent_version(create_context_params.user_agent_version());
  }
  if (create_context_params.has_remote_debugging_port()) {
    output.set_remote_debugging_port(
        create_context_params.remote_debugging_port());
  }
  return output;
}

bool IsPermissionGrantedInAppConfig(
    const chromium::cast::ApplicationConfig& application_config,
    fuchsia::web::PermissionType permission_type) {
  if (application_config.has_permissions()) {
    for (auto& permission : application_config.permissions()) {
      if (permission.has_type() && permission.type() == permission_type)
        return true;
    }
  }
  return false;
}

}  // namespace

CastRunner::CastRunner(
    WebContentRunner::GetContextParamsCallback get_context_params_callback,
    bool is_headless)
    : WebContentRunner(base::BindRepeating(&CastRunner::GetMainContextParams,
                                           base::Unretained(this))),
      get_context_params_callback_(std::move(get_context_params_callback)),
      is_headless_(is_headless),
      service_directory_(CreateServiceDirectory()) {}

CastRunner::~CastRunner() = default;

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

  pending_components_.emplace(std::make_unique<PendingCastComponent>(
      this,
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info)),
      std::move(controller_request), cast_url.GetContent()));
}

void CastRunner::DestroyComponent(WebComponent* component) {
  WebContentRunner::DestroyComponent(component);

  if (component == audio_capturer_component_)
    audio_capturer_component_ = nullptr;

  if (on_component_destroyed_callback_) {
    // |this| may be deleted and should not be used after this line.
    std::move(on_component_destroyed_callback_).Run(this);
  }
}

std::unique_ptr<base::fuchsia::FilteredServiceDirectory>
CastRunner::CreateServiceDirectory() {
  auto service_directory =
      std::make_unique<base::fuchsia::FilteredServiceDirectory>(
          base::fuchsia::ComponentContextForCurrentProcess()->svc().get());

  for (auto* name : kServices) {
    service_directory->AddService(name);
  }

  // Handle fuchsia.media.Audio requests so we can redirect to the agent if
  // necessary.
  service_directory->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>([this](zx::channel channel,
                                            async_dispatcher_t* dispatcher) {
        this->ConnectAudioProtocol(
            fidl::InterfaceRequest<fuchsia::media::Audio>(std::move(channel)));
      }),
      fuchsia::media::Audio::Name_);

  // Proxy fuchsia.legacymetrics.MetricsRecorder connection requests to the
  // Agent.
  service_directory->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>(
          [this](zx::channel channel, async_dispatcher_t*) {
            this->ConnectMetricsRecorderProtocol(
                fidl::InterfaceRequest<fuchsia::legacymetrics::MetricsRecorder>(
                    std::move(channel)));
          }),
      fuchsia::legacymetrics::MetricsRecorder::Name_);

  return service_directory;
}

void CastRunner::LaunchPendingComponent(PendingCastComponent* pending_component,
                                        CastComponent::Params params) {
  // The runner which will host the newly created CastComponent.
  CastRunner* component_owner = this;

  if (params.application_config
          .has_content_directories_for_isolated_application()) {
    // Create an isolated CastRunner instance which will own the
    // CastComponent.
    component_owner = CreateChildRunnerForIsolatedComponent(&params);
  }

  component_owner->CreateAndRegisterCastComponent(std::move(params));
  pending_components_.erase(pending_component);
}

void CastRunner::CancelPendingComponent(
    PendingCastComponent* pending_component) {
  size_t count = pending_components_.erase(pending_component);
  DCHECK_EQ(count, 1u);
}

void CastRunner::CreateAndRegisterCastComponent(
    CastComponent::Params component_params) {
  GURL app_url = GURL(component_params.application_config.web_url());
  auto cast_component =
      std::make_unique<CastComponent>(this, std::move(component_params));
  cast_component->StartComponent();
  cast_component->LoadUrl(std::move(app_url),
                          std::vector<fuchsia::net::http::Header>());

  if (IsPermissionGrantedInAppConfig(
          cast_component->application_config(),
          fuchsia::web::PermissionType::MICROPHONE)) {
    audio_capturer_component_ = cast_component.get();
  }

  RegisterComponent(std::move(cast_component));
}

CastRunner* CastRunner::CreateChildRunnerForIsolatedComponent(
    CastComponent::Params* component_params) {
  // Construct the CreateContextParams in order to create a new Context.  Some
  // common parameters must be inherited from default params returned from
  // |get_context_params_callback_|.
  fuchsia::web::CreateContextParams isolated_context_params =
      BuildCreateContextParamsForIsolatedRunners(
          get_context_params_callback_.Run());

  isolated_context_params.set_content_directories(
      std::move(*component_params->application_config
                     .mutable_content_directories_for_isolated_application()));

  auto create_context_params_callback = base::BindRepeating(
      [](fuchsia::web::CreateContextParams isolated_context_params) {
        return isolated_context_params;
      },
      base::Passed(std::move(isolated_context_params)));

  auto cast_runner = std::make_unique<CastRunner>(
      std::move(create_context_params_callback), is_headless());
  cast_runner->on_component_destroyed_callback_ = base::BindOnce(
      &CastRunner::OnIsolatedRunnerEmpty, base::Unretained(this));

  CastRunner* cast_runner_ptr = cast_runner.get();
  isolated_runners_.insert(std::move(cast_runner));
  return cast_runner_ptr;
}

void CastRunner::OnIsolatedRunnerEmpty(CastRunner* runner) {
  auto runner_iterator = isolated_runners_.find(runner);
  DCHECK(runner_iterator != isolated_runners_.end());

  isolated_runners_.erase(runner_iterator);
}

void CastRunner::ConnectAudioProtocol(
    fidl::InterfaceRequest<fuchsia::media::Audio> request) {
  // If we have a component that allows AudioCapturer access then redirect the
  // fuchsia.media.Audio requests to the corresponding agent.
  if (audio_capturer_component_) {
    audio_capturer_component_->agent_manager()->ConnectToAgentService(
        audio_capturer_component_->application_config().agent_url(),
        std::move(request));
    return;
  }

  // Otherwise use the default fuchsia.media.Audio implementation.
  base::fuchsia::ComponentContextForCurrentProcess()->svc()->Connect(
      std::move(request));
}

void CastRunner::ConnectMetricsRecorderProtocol(
    fidl::InterfaceRequest<fuchsia::legacymetrics::MetricsRecorder> request) {
  CastComponent* component =
      reinterpret_cast<CastComponent*>(GetAnyComponent());
  DCHECK(component);

  component->agent_manager()->ConnectToAgentService(
      component->application_config().agent_url(), std::move(request));
}

fuchsia::web::CreateContextParams CastRunner::GetMainContextParams() {
  fuchsia::web::CreateContextParams create_context_params =
      get_context_params_callback_.Run();

  // Override with the custom service directory.
  service_directory_->ConnectClient(
      create_context_params.mutable_service_directory()->NewRequest());

  return create_context_params;
}
