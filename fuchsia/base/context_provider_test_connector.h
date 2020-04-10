// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_CONTEXT_PROVIDER_TEST_CONNECTOR_H_
#define FUCHSIA_BASE_CONTEXT_PROVIDER_TEST_CONNECTOR_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/command_line.h"

namespace cr_fuchsia {

fidl::InterfaceHandle<fuchsia::io::Directory> StartWebEngineForTests(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line =
        base::CommandLine(base::CommandLine::NO_PROGRAM));

// TODO(crbug.com/1046615): Use test manifests for package specification.
fuchsia::web::ContextProviderPtr ConnectContextProvider(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line =
        base::CommandLine(base::CommandLine::NO_PROGRAM));

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_CONTEXT_PROVIDER_TEST_CONNECTOR_H_
