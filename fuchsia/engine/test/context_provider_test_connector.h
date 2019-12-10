// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_TEST_CONTEXT_PROVIDER_TEST_CONNECTOR_H_
#define FUCHSIA_ENGINE_TEST_CONTEXT_PROVIDER_TEST_CONNECTOR_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include "base/command_line.h"

fidl::InterfaceHandle<fuchsia::web::ContextProvider> ConnectContextProvider(
    fidl::InterfaceRequest<fuchsia::web::ContextProvider>
        context_provider_request,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line =
        base::CommandLine(base::CommandLine::NO_PROGRAM));

#endif  // FUCHSIA_ENGINE_TEST_CONTEXT_PROVIDER_TEST_CONNECTOR_H_
