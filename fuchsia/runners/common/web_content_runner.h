// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
#define FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/macros.h"
#include "base/optional.h"

class WebComponent;

// sys::Runner that instantiates components hosting standard web content.
class WebContentRunner : public fuchsia::sys::Runner {
 public:
  using GetContextParamsCallback =
      base::RepeatingCallback<fuchsia::web::CreateContextParams()>;

  // Creates a Runner for web-based components, and publishes it to the
  // specified OutgoingDirectory.
  // |get_context_params_callback|: Returns parameters for the Runner's
  //     web.Context.
  // |outgoing_directory|: The directory that the Runner's services will be
  //     published to.
  WebContentRunner(GetContextParamsCallback get_context_params_callback,
                   sys::OutgoingDirectory* outgoing_directory);

  // Creates a Runner which launches components using the specified |context|.
  // The caller may publish the Runner, or call StartComponent() manually to
  // create new components with it.
  explicit WebContentRunner(fuchsia::web::ContextPtr context);

  ~WebContentRunner() override;

  // TODO(crbug.com/1046615): Make this static when the injected ContextProvider
  // goes away.
  fuchsia::web::ContextPtr CreateWebContext(
      fuchsia::web::CreateContextParams context_params);

  // Gets a pointer to this runner's Context, creating one if needed.
  fuchsia::web::Context* GetContext();

  // Used by WebComponent instances to signal that the ComponentController
  // channel was dropped, and therefore the component should be destroyed.
  virtual void DestroyComponent(WebComponent* component);

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

  // Used by tests to asynchronously access the first WebComponent.
  void SetWebComponentCreatedCallbackForTest(
      base::RepeatingCallback<void(WebComponent*)> callback);

  // Registers a WebComponent, or specialization, with this Runner.
  void RegisterComponent(std::unique_ptr<WebComponent> component);

  // Overrides the environment's the ContextProvider to use.
  // TODO(crbug.com/1046615): Use test manifests for package specification.
  void SetContextProviderForTest(
      fuchsia::web::ContextProviderPtr context_provider);

  // Disconnects the Context used by this Runner.
  void DisconnectContextForTest();

 protected:
  base::RepeatingCallback<void(WebComponent*)>
  web_component_created_callback_for_test() const {
    return web_component_created_callback_for_test_;
  }

 private:
  fuchsia::web::ContextProvider* GetContextProvider();

  const GetContextParamsCallback get_context_params_callback_;

  // If set, invoked whenever a WebComponent is created.
  base::RepeatingCallback<void(WebComponent*)>
      web_component_created_callback_for_test_;

  fuchsia::web::ContextProviderPtr context_provider_;
  fuchsia::web::ContextPtr context_;
  std::set<std::unique_ptr<WebComponent>, base::UniquePtrComparator>
      components_;

  // Publishes this Runner into the service directory specified at construction.
  // This is not set for child runner instances.
  base::Optional<base::fuchsia::ScopedServiceBinding<fuchsia::sys::Runner>>
      service_binding_;

  DISALLOW_COPY_AND_ASSIGN(WebContentRunner);
};

#endif  // FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
