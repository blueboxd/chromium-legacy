// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
#define FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/startup_context.h"
#include "base/macros.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_component.h"
#include "fuchsia/runners/common/web_content_runner.h"

// sys::Runner which instantiates Cast activities specified via cast/casts URIs.
class CastRunner : public WebContentRunner {
 public:
  CastRunner(sys::OutgoingDirectory* outgoing_directory,
             fuchsia::web::ContextPtr context);

  ~CastRunner() override;

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

  // Used to connect to the CastAgent to access Cast-specific services.
  static const char kAgentComponentUrl[];

 private:
  void GetConfigCallback(CastComponent::CastComponentParams* pending_component,
                         chromium::cast::ApplicationConfig app_config);
  void GetBindingsCallback(
      CastComponent::CastComponentParams* pending_component,
      std::vector<chromium::cast::ApiBinding> bindings);

  // Starts a component once all configuration data is available.
  void MaybeStartComponent(
      CastComponent::CastComponentParams* pending_component);

  // Holds StartComponent() requests while the ApplicationConfig is being
  // fetched from the ApplicationConfigManager.
  base::flat_set<std::unique_ptr<CastComponent::CastComponentParams>,
                 base::UniquePtrComparator>
      pending_components_;

  DISALLOW_COPY_AND_ASSIGN(CastRunner);
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
