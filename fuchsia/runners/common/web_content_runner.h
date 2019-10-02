// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
#define FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <set>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/macros.h"

class WebComponent;

// sys::Runner that instantiates components hosting standard web content.
class WebContentRunner : public fuchsia::sys::Runner {
 public:
  // Creates and returns a web.Context with a default path and parameters,
  // and with access to the same services as this Runner. The returned binding
  // is configured to exit this process on error.
  static fuchsia::web::ContextPtr CreateDefaultWebContext(
      fuchsia::web::ContextFeatureFlags features);

  // Creates and returns a web.Context built with |create_params|.
  // The returned binding is configured to exit this process on error.
  static fuchsia::web::ContextPtr CreateWebContext(
      fuchsia::web::CreateContextParams create_params);

  // Returns a CreateContextParams that can be used as a base for creating a
  // web.Context with a custom configuration.
  // An incognito web.Context will be created if |data_directory| is unbound.
  static fuchsia::web::CreateContextParams BuildCreateContextParams(
      fidl::InterfaceHandle<fuchsia::io::Directory> data_directory,
      fuchsia::web::ContextFeatureFlags features);

  // |outgoing_directory|: OutgoingDirectory into which this Runner will be
  //   published. |on_idle_closure| will be invoked when the final client of the
  //   published service disconnects, even if one or more Components are still
  //   active.
  // |context|: Context (e.g. persisted profile storage) under which all web
  //   content launched through this Runner instance will be run.
  WebContentRunner(sys::OutgoingDirectory* outgoing_directory,
                   fuchsia::web::ContextPtr context);
  ~WebContentRunner() override;

  fuchsia::web::Context* context() { return context_.get(); }

  // Used by WebComponent instances to signal that the ComponentController
  // channel was dropped, and therefore the component should be destroyed.
  void DestroyComponent(WebComponent* component);

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

  // Used by tests to asynchronously access the first WebComponent.
  void GetWebComponentForTest(base::OnceCallback<void(WebComponent*)> callback);

 protected:
  // Registers a WebComponent, or specialization, with this Runner.
  void RegisterComponent(std::unique_ptr<WebComponent> component);

 private:
  fuchsia::web::ContextPtr context_;
  std::set<std::unique_ptr<WebComponent>, base::UniquePtrComparator>
      components_;

  // Publishes this Runner into the service directory specified at construction.
  base::fuchsia::ScopedServiceBinding<fuchsia::sys::Runner> service_binding_;

  // Test-only callback for GetWebComponentForTest.
  base::OnceCallback<void(WebComponent*)> web_component_test_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebContentRunner);
};

#endif  // FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
