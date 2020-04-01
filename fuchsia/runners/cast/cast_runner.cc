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

    // Redirected to the agent.
    // fuchsia.media.Audio
};

bool AreCastComponentParamsValid(
    const CastComponent::CastComponentParams& params) {
  return !params.app_config.IsEmpty() &&
         params.api_bindings_client->HasBindings() &&
         params.rewrite_rules.has_value() &&
         params.media_session_id.has_value();
}

// Creates a CreateContextParams object which can be used as a basis
// for starting isolated Runners.
fuchsia::web::CreateContextParams BuildCreateContextParamsForIsolatedRunners(
    const fuchsia::web::CreateContextParams& create_context_params) {
  fuchsia::web::CreateContextParams output;

  // Isolated contexts receive only a limited set of features.
  fuchsia::web::ContextFeatureFlags features =
      fuchsia::web::ContextFeatureFlags::AUDIO;

  if ((create_context_params.features() &
       fuchsia::web::ContextFeatureFlags::HEADLESS) ==
      fuchsia::web::ContextFeatureFlags::HEADLESS) {
    features |= fuchsia::web::ContextFeatureFlags::HEADLESS;
  } else {
    features |= fuchsia::web::ContextFeatureFlags::VULKAN |
                fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
                fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;
  }

  // The rest of |create_context_params.features()| is ignored.
  // TODO(crbug.com/1059497): Respect the flags or don't pass them in tests.

  output.set_features(features);

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
    const chromium::cast::ApplicationConfig& app_config,
    fuchsia::web::PermissionType permission_type) {
  if (app_config.has_permissions()) {
    for (auto& permission : app_config.permissions()) {
      if (permission.has_type() && permission.type() == permission_type)
        return true;
    }
  }
  return false;
}

}  // namespace

const char CastRunner::kAgentComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/cast_agent#meta/cast_agent.cmx";

CastRunner::CastRunner(fuchsia::web::CreateContextParams create_context_params,
                       sys::OutgoingDirectory* outgoing_directory)
    : WebContentRunner(std::move(create_context_params), outgoing_directory),
      common_create_context_params_(
          BuildCreateContextParamsForIsolatedRunners(create_params_)) {
  InitializeServiceDirectory();
}

CastRunner::CastRunner(OnDestructionCallback on_destruction_callback,
                       fuchsia::web::ContextPtr context,
                       bool is_headless)
    : WebContentRunner(std::move(context), is_headless),
      on_destruction_callback_(std::move(on_destruction_callback)) {}

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

  // The application configuration is obtained asynchronously via the
  // per-component ApplicationConfigManager. The pointer to that service must be
  // kept live until the request completes or CastRunner is deleted.
  auto pending_component =
      std::make_unique<CastComponent::CastComponentParams>();
  pending_component->startup_context =
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info));
  pending_component->agent_manager = std::make_unique<cr_fuchsia::AgentManager>(
      pending_component->startup_context->component_context()->svc().get());
  pending_component->controller_request = std::move(controller_request);

  // Request the configuration for this application from the app_config_manager.
  // This will return the configuration for the application, as well as the
  // agent that should handle this application.
  pending_component->startup_context->svc()->Connect(
      pending_component->app_config_manager.NewRequest());
  pending_component->app_config_manager.set_error_handler(
      [this, pending_component = pending_component.get()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "ApplicationConfigManager disconnected.";
        CancelComponentLaunch(pending_component);
      });
  const std::string cast_app_id(cast_url.GetContent());
  pending_component->app_config_manager->GetConfig(
      cast_app_id, [this, pending_component = pending_component.get()](
                       chromium::cast::ApplicationConfig app_config) {
        GetConfigCallback(pending_component, std::move(app_config));
      });

  pending_component->application_context =
      pending_component->agent_manager
          ->ConnectToAgentService<chromium::cast::ApplicationContext>(
              CastRunner::kAgentComponentUrl);
  pending_component->application_context.set_error_handler(
      [this, pending_component = pending_component.get()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "ApplicationContext disconnected.";
        if (!pending_component->media_session_id) {
          pending_component->media_session_id = 0;
          MaybeStartComponent(pending_component);
        }
      });
  pending_component->application_context->GetMediaSessionId(
      [this, pending_component = pending_component.get()](uint64_t session_id) {
        pending_component->media_session_id = session_id;
        MaybeStartComponent(pending_component);
      });

  pending_components_.emplace(std::move(pending_component));
}

void CastRunner::DestroyComponent(WebComponent* component) {
  WebContentRunner::DestroyComponent(component);

  if (component == audio_capturer_component_)
    audio_capturer_component_ = nullptr;

  if (on_destruction_callback_) {
    // |this| may be deleted and should not be used after this line.
    std::move(on_destruction_callback_).Run(this);
  }
}

void CastRunner::GetConfigCallback(
    CastComponent::CastComponentParams* pending_component,
    chromium::cast::ApplicationConfig app_config) {
  auto it = pending_components_.find(pending_component);
  DCHECK(it != pending_components_.end());

  if (app_config.IsEmpty()) {
    pending_components_.erase(it);
    DLOG(WARNING) << "No application config was found.";
    return;
  }

  if (!app_config.has_web_url()) {
    pending_components_.erase(it);
    DLOG(WARNING) << "Only web-based applications are supported.";
    return;
  }

  if (!app_config.has_agent_url()) {
    pending_components_.erase(it);
    DLOG(WARNING) << "No agent has been associated with this app.";
    return;
  }

  pending_component->app_config = std::move(app_config);

  // Request binding details from the Agent.
  fidl::InterfaceHandle<chromium::cast::ApiBindings> api_bindings_client;
  pending_component->agent_manager->ConnectToAgentService(
      pending_component->app_config.agent_url(),
      api_bindings_client.NewRequest());
  pending_component->api_bindings_client = std::make_unique<ApiBindingsClient>(
      std::move(api_bindings_client),
      base::BindOnce(&CastRunner::MaybeStartComponent, base::Unretained(this),
                     base::Unretained(pending_component)),
      base::BindOnce(&CastRunner::CancelComponentLaunch, base::Unretained(this),
                     base::Unretained(pending_component)));

  // Request UrlRequestRewriteRulesProvider from the Agent.
  pending_component->agent_manager->ConnectToAgentService(
      pending_component->app_config.agent_url(),
      pending_component->rewrite_rules_provider.NewRequest());
  pending_component->rewrite_rules_provider.set_error_handler(
      [this, pending_component = pending_component](zx_status_t status) {
        if (status != ZX_ERR_PEER_CLOSED) {
          ZX_LOG(ERROR, status)
              << "UrlRequestRewriteRulesProvider disconnected.";
          CancelComponentLaunch(pending_component);
          return;
        }
        ZX_LOG(WARNING, status)
            << "UrlRequestRewriteRulesProvider unsupported.";
        pending_component->rewrite_rules =
            std::vector<fuchsia::web::UrlRequestRewriteRule>();
        MaybeStartComponent(pending_component);
      });
  pending_component->rewrite_rules_provider->GetUrlRequestRewriteRules(
      [this, pending_component = pending_component](
          std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
        pending_component->rewrite_rules =
            base::Optional<std::vector<fuchsia::web::UrlRequestRewriteRule>>(
                std::move(rewrite_rules));
        MaybeStartComponent(pending_component);
      });
}

void CastRunner::InitializeServiceDirectory() {
  service_directory_ =
      std::make_unique<base::fuchsia::FilteredServiceDirectory>(
          base::fuchsia::ComponentContextForCurrentProcess()->svc().get());

  for (auto* name : kServices) {
    service_directory_->AddService(name);
  }

  // Handle fuchsia.media.Audio requests so we can redirect to the agent if
  // necessary.
  service_directory_->outgoing_directory()->AddPublicService(
      std::make_unique<vfs::Service>([this](zx::channel channel,
                                            async_dispatcher_t* dispatcher) {
        this->ConnectAudioProtocol(
            fidl::InterfaceRequest<fuchsia::media::Audio>(std::move(channel)));
      }),
      fuchsia::media::Audio::Name_);

  fidl::InterfaceHandle<fuchsia::io::Directory> client_handle;
  service_directory_->ConnectClient(client_handle.NewRequest());
  create_params_.set_service_directory(std::move(client_handle));
}

void CastRunner::MaybeStartComponent(
    CastComponent::CastComponentParams* pending_component_params) {
  if (!AreCastComponentParamsValid(*pending_component_params))
    return;

  // The runner which will host the newly created CastComponent.
  CastRunner* component_owner = this;
  if (pending_component_params->app_config
          .has_content_directories_for_isolated_application()) {
    // Create a isolated, isolated CastRunner instance which will own the
    // CastComponent.
    component_owner =
        CreateChildRunnerForIsolatedComponent(pending_component_params);

    if (!component_owner)
      return;
  }

  component_owner->CreateAndRegisterCastComponent(
      std::move(*pending_component_params));
  pending_components_.erase(pending_component_params);
}

void CastRunner::CancelComponentLaunch(
    CastComponent::CastComponentParams* params) {
  size_t count = pending_components_.erase(params);
  DCHECK_EQ(count, 1u);
}

void CastRunner::CreateAndRegisterCastComponent(
    CastComponent::CastComponentParams params) {
  GURL app_url = GURL(params.app_config.web_url());
  auto cast_component =
      std::make_unique<CastComponent>(this, std::move(params));
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
    CastComponent::CastComponentParams* params) {
  // Construct the CreateContextParams in order to create a new Context.
  // Some common parameters must be inherited from
  // |common_create_context_params_|.
  fuchsia::web::CreateContextParams isolated_context_params;
  zx_status_t status =
      common_create_context_params_.Clone(&isolated_context_params);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "clone";
    return nullptr;
  }

  // Service redirection is not necessary for isolated context. Pass default
  // /svc as is, without overriding any services.
  isolated_context_params.set_service_directory(base::fuchsia::OpenDirectory(
      base::FilePath(base::fuchsia::kServiceDirectoryPath)));
  DCHECK(isolated_context_params.service_directory());

  isolated_context_params.set_content_directories(
      std::move(*params->app_config
                     .mutable_content_directories_for_isolated_application()));

  std::unique_ptr<CastRunner> cast_runner(new CastRunner(
      base::BindOnce(&CastRunner::OnChildRunnerDestroyed,
                     base::Unretained(this)),
      CreateWebContext(std::move(isolated_context_params)), is_headless()));

  // If test code is listening for Component creation events, then wire up the
  // isolated CastRunner to signal component creation events.
  if (web_component_created_callback_for_test()) {
    cast_runner->SetWebComponentCreatedCallbackForTest(
        web_component_created_callback_for_test());
  }

  CastRunner* cast_runner_ptr = cast_runner.get();
  isolated_runners_.insert(std::move(cast_runner));
  return cast_runner_ptr;
}

void CastRunner::OnChildRunnerDestroyed(CastRunner* runner) {
  auto runner_iterator = isolated_runners_.find(runner);
  DCHECK(runner_iterator != isolated_runners_.end());

  isolated_runners_.erase(runner_iterator);
}

size_t CastRunner::GetChildCastRunnerCountForTest() {
  return isolated_runners_.size();
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