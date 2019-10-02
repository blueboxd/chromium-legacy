// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/web_content_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/startup_context.h"
#include "base/logging.h"
#include "fuchsia/runners/buildflags.h"
#include "fuchsia/runners/common/web_component.h"
#include "url/gurl.h"

namespace {

fidl::InterfaceHandle<fuchsia::io::Directory> OpenDirectoryOrFail(
    const base::FilePath& path) {
  auto directory = base::fuchsia::OpenDirectory(path);
  CHECK(directory) << "Failed to open " << path;
  return directory;
}

}  // namespace

// static
fuchsia::web::ContextPtr WebContentRunner::CreateWebContext(
    fuchsia::web::CreateContextParams create_params) {
  auto web_context_provider = base::fuchsia::ComponentContextForCurrentProcess()
                                  ->svc()
                                  ->Connect<fuchsia::web::ContextProvider>();

  fuchsia::web::ContextPtr web_context;
  web_context_provider->Create(std::move(create_params),
                               web_context.NewRequest());
  web_context.set_error_handler([](zx_status_t status) {
    // If the browser instance died, then exit everything and do not attempt
    // to recover. appmgr will relaunch the runner when it is needed again.
    ZX_LOG(ERROR, status) << "Connection to Context lost.";
    exit(1);
  });
  return web_context;
}

// static
fuchsia::web::ContextPtr WebContentRunner::CreateDefaultWebContext(
    fuchsia::web::ContextFeatureFlags features) {
  return CreateWebContext(BuildCreateContextParams(
      OpenDirectoryOrFail(
          base::FilePath(base::fuchsia::kPersistedDataDirectoryPath)),
      features));
}

// static
fuchsia::web::CreateContextParams WebContentRunner::BuildCreateContextParams(
    fidl::InterfaceHandle<fuchsia::io::Directory> data_directory,
    fuchsia::web::ContextFeatureFlags features) {
  fuchsia::web::CreateContextParams create_params;
  create_params.set_service_directory(OpenDirectoryOrFail(
      base::FilePath(base::fuchsia::kServiceDirectoryPath)));

  if (data_directory)
    create_params.set_data_directory(std::move(data_directory));

  // Set |remote_debugging_port| on the context, if set.
  if (BUILDFLAG(ENABLE_REMOTE_DEBUGGING_ON_PORT) != 0) {
    create_params.set_remote_debugging_port(
        BUILDFLAG(ENABLE_REMOTE_DEBUGGING_ON_PORT));
  }

  return create_params;
}

WebContentRunner::WebContentRunner(sys::OutgoingDirectory* outgoing_directory,
                                   fuchsia::web::ContextPtr context)
    : context_(std::move(context)), service_binding_(outgoing_directory, this) {
  DCHECK(context_);
}

WebContentRunner::~WebContentRunner() = default;

void WebContentRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  GURL url(package.resolved_url);
  if (!url.is_valid()) {
    LOG(ERROR) << "Rejected invalid URL: " << url;
    return;
  }

  std::unique_ptr<WebComponent> component = std::make_unique<WebComponent>(
      this,
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info)),
      std::move(controller_request));
  component->LoadUrl(url, std::vector<fuchsia::net::http::Header>());
  RegisterComponent(std::move(component));
}

void WebContentRunner::GetWebComponentForTest(
    base::OnceCallback<void(WebComponent*)> callback) {
  if (!components_.empty()) {
    std::move(callback).Run(components_.begin()->get());
    return;
  }
  web_component_test_callback_ = std::move(callback);
}

void WebContentRunner::DestroyComponent(WebComponent* component) {
  components_.erase(components_.find(component));
}

void WebContentRunner::RegisterComponent(
    std::unique_ptr<WebComponent> component) {
  if (web_component_test_callback_) {
    std::move(web_component_test_callback_).Run(component.get());
  }
  components_.insert(std::move(component));
}
