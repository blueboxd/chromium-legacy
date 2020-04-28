// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/context_provider_test_connector.h"

#include <unistd.h>

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/sys/cpp/component_context.h>
#include <zircon/processargs.h>
#include <utility>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/strcat.h"
#include "fuchsia/base/release_channel.h"

namespace cr_fuchsia {

fidl::InterfaceHandle<fuchsia::io::Directory> StartWebEngineForTests(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line) {
  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = base::StrCat({"fuchsia-pkg://fuchsia.com/web_engine",
                                  BUILDFLAG(FUCHSIA_RELEASE_CHANNEL_SUFFIX),
                                  "#meta/context_provider.cmx"});
  launch_info.arguments = command_line.argv();

  // Clone stderr from the current process to WebEngine and ask it to
  // redirects all logs to stderr.
  launch_info.err = fuchsia::sys::FileDescriptor::New();
  launch_info.err->type0 = PA_FD;
  zx_status_t status = fdio_fd_clone(
      STDERR_FILENO, launch_info.err->handle0.reset_and_get_address());
  ZX_CHECK(status == ZX_OK, status);
  launch_info.arguments->push_back("--enable-logging=stderr");

  fidl::InterfaceHandle<fuchsia::io::Directory> web_engine_services_dir;
  launch_info.directory_request =
      web_engine_services_dir.NewRequest().TakeChannel();

  fuchsia::sys::LauncherPtr launcher;
  base::fuchsia::ComponentContextForCurrentProcess()->svc()->Connect(
      launcher.NewRequest());
  launcher->CreateComponent(std::move(launch_info),
                            std::move(component_controller_request));

  return web_engine_services_dir;
}

fuchsia::web::ContextProviderPtr ConnectContextProvider(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line) {
  sys::ServiceDirectory web_engine_service_dir(StartWebEngineForTests(
      std::move(component_controller_request), command_line));
  return web_engine_service_dir.Connect<fuchsia::web::ContextProvider>();
}

}  // namespace cr_fuchsia
