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
  using CreateContextCallback = base::OnceCallback<fuchsia::web::ContextPtr()>;

  // |create_params|: Parameters to use for the Runner's web.Context.
  // |outgoing_directory|: The directory that the Runner's services will be
  //                       published to.
  WebContentRunner(fuchsia::web::CreateContextParams create_params,
                   sys::OutgoingDirectory* outgoing_directory);

  // Alternative constructor for unpublished Runners.
  explicit WebContentRunner(fuchsia::web::ContextPtr context, bool is_headless);

  ~WebContentRunner() override;

  // TODO(crbug.com/1046615): Make this static when the injected ContextProvider
  // goes away.
  fuchsia::web::ContextPtr CreateWebContext(
      fuchsia::web::CreateContextParams create_params);

  // Gets a pointer to this runner's Context, creating one if needed.
  fuchsia::web::Context* GetContext();

  // Used by WebComponent instances to signal that the ComponentController
  // channel was dropped, and therefore the component should be destroyed.
  virtual void DestroyComponent(WebComponent* component);

  // Set if Cast applications are to be run without graphical rendering.
  bool is_headless() const { return is_headless_; }

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

 protected:
  base::RepeatingCallback<void(WebComponent*)>
  web_component_created_callback_for_test() const {
    return web_component_created_callback_for_test_;
  }

  fuchsia::web::CreateContextParams create_params_;

 private:
  fuchsia::web::ContextProvider* GetContextProvider();

  // If set, invoked whenever a WebComponent is created.
  base::RepeatingCallback<void(WebComponent*)>
      web_component_created_callback_for_test_;

  fuchsia::web::ContextProviderPtr context_provider_;
  fuchsia::web::ContextPtr context_;
  std::set<std::unique_ptr<WebComponent>, base::UniquePtrComparator>
      components_;
  const bool is_headless_;

  // Publishes this Runner into the service directory specified at construction.
  // This is not set for child runner instances.
  base::Optional<base::fuchsia::ScopedServiceBinding<fuchsia::sys::Runner>>
      service_binding_;

  DISALLOW_COPY_AND_ASSIGN(WebContentRunner);
};

#endif  // FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
