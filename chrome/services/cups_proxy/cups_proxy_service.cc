// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/cups_proxy_service.h"

#include <string>
#include <utility>
#include <vector>

#include "chrome/services/cups_proxy/cups_proxy_service_delegate.h"

namespace chromeos {
namespace printing {

CupsProxyService::CupsProxyService(
    service_manager::mojom::ServiceRequest request,
    std::unique_ptr<CupsProxyServiceDelegate> delegate)
    : service_binding_(this, std::move(request)),
      delegate_(std::move(delegate)) {}

CupsProxyService::~CupsProxyService() = default;

void CupsProxyService::OnStart() {
  BindToCupsProxyDaemon();
}

void CupsProxyService::OnConnect(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DLOG(WARNING) << "CupsProxyService incorrectly received interface_request";
}

void CupsProxyService::BindToCupsProxyDaemon() {
  // TODO(crbug.com/945409): Implement this.
}

void CupsProxyService::OnBindToCupsProxyDaemon(const bool success) {
  if (!success) {
    DLOG(WARNING) << "CupsProxyDaemonConnection bootstrap failed";
  }
}

}  // namespace printing
}  // namespace chromeos
