// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/cast_ui_devtools.h"

#include "components/ui_devtools/connector_delegate.h"
#include "components/ui_devtools/views/devtools_server_util.h"

namespace chromecast {
namespace shell {

CastUIDevTools::CastUIDevTools(network::mojom::NetworkContext* network_context)
    : devtools_server_(CreateServer(network_context)) {}

CastUIDevTools::~CastUIDevTools() = default;

std::unique_ptr<ui_devtools::UiDevToolsServer> CastUIDevTools::CreateServer(
    network::mojom::NetworkContext* network_context) const {
  return ui_devtools::CreateUiDevToolsServerForViews(
      network_context, nullptr /* connector_delegate */);
}

}  // namespace shell
}  // namespace chromecast
