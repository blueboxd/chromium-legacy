// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
#define FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/startup_context.h"
#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_component.h"
#include "fuchsia/runners/common/web_content_runner.h"

namespace base {
namespace fuchsia {
class FilteredServiceDirectory;
}  // namespace fuchsia
}  // namespace base

// sys::Runner which instantiates Cast activities specified via cast/casts URIs.
class CastRunner : public WebContentRunner {
 public:
  // Creates a Runner for Cast components and publishes it into the specified
  // OutgoingDirectory.
  // |get_context_params_callback|: Returns the context parameters to use.
  // |is_headless|: True if |get_context_params_callback| sets the HEADLESS
  //     feature flag.
  // |outgoing_directory|: The directory that this CastRunner will publish
  //     itself to.
  CastRunner(GetContextParamsCallback get_context_params_callback,
             bool is_headless,
             sys::OutgoingDirectory* outgoing_directory);
  ~CastRunner() override;

  CastRunner(const CastRunner&) = delete;
  CastRunner& operator=(const CastRunner&) = delete;

  // WebContentRunner implementation.
  void DestroyComponent(WebComponent* component) override;

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

  // Used to connect to the CastAgent to access Cast-specific services.
  static const char kAgentComponentUrl[];

  // Returns true if this Runner is configured not to use Scenic.
  bool is_headless() const { return is_headless_; }

  // Returns the number of active CastRunner instances.
  size_t GetChildCastRunnerCountForTest();

 private:
  using OnDestructionCallback = base::OnceCallback<void(CastRunner*)>;

  // Constructor used for creating CastRunners that run apps in dedicated
  // Contexts. Child CastRunners may only spawn one Component and will be
  // destroyed by their parents when their singleton Components are destroyed.
  // |on_destruction_callback| is invoked when the child component is destroyed.
  CastRunner(OnDestructionCallback on_destruction_callback,
             fuchsia::web::ContextPtr context,
             bool is_headless);

  // Creates and returns the service directory that is passed to the main web
  // context.
  std::unique_ptr<base::fuchsia::FilteredServiceDirectory>
  CreateServiceDirectory();

  // Starts a component once all configuration data is available.
  void MaybeStartComponent(
      CastComponent::CastComponentParams* pending_component_params);

  // Cancels the launch of a component.
  void CancelComponentLaunch(
      CastComponent::CastComponentParams* pending_component_params);

  void CreateAndRegisterCastComponent(
      CastComponent::CastComponentParams component_params);
  void OnApplicationConfigReceived(
      CastComponent::CastComponentParams* pending_component_params,
      chromium::cast::ApplicationConfig app_config);
  void OnChildRunnerDestroyed(CastRunner* cast_runner);

  // Creates a CastRunner configured to serve data from content directories in
  // |params|. Returns nullptr if an error occurred during CastRunner creation.
  CastRunner* CreateChildRunnerForIsolatedComponent(
      CastComponent::CastComponentParams* component_params);

  // Handler for fuchsia.media.Audio requests in |service_directory_|.
  void ConnectAudioProtocol(
      fidl::InterfaceRequest<fuchsia::media::Audio> request);

  // Returns the parameters with which the main context should be created.
  fuchsia::web::CreateContextParams GetMainContextParams();

  const WebContentRunner::GetContextParamsCallback get_context_params_callback_;

  // True if this Runner uses Context(s) with the HEADLESS feature set.
  const bool is_headless_;

  // Holds StartComponent() requests while the ApplicationConfig is being
  // fetched from the ApplicationConfigManager.
  base::flat_set<std::unique_ptr<CastComponent::CastComponentParams>,
                 base::UniquePtrComparator>
      pending_components_;

  // Used as a template for creating the ContextPtrs of isolated Runners.
  fuchsia::web::CreateContextParams common_create_context_params_;

  // Invoked upon destruction of "isolated" runners, used to signal termination
  // to parents.
  OnDestructionCallback on_destruction_callback_;

  // Manages isolated CastRunners owned by |this| instance.
  base::flat_set<std::unique_ptr<CastRunner>, base::UniquePtrComparator>
      isolated_runners_;

  // ServiceDirectory passed to the main Context, to allow Audio capturer
  // service requests to be routed to the relevant Agent.
  const std::unique_ptr<base::fuchsia::FilteredServiceDirectory>
      service_directory_;

  // Last component that was created with permission to access MICROPHONE.
  CastComponent* audio_capturer_component_ = nullptr;
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
