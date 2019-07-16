// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_proxy_service_manager.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/services/cups_proxy/public/mojom/constants.mojom.h"
#include "chromeos/dbus/cups_proxy/cups_proxy_client.h"
#include "content/public/browser/browser_context.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

CupsProxyServiceManager::CupsProxyServiceManager() : weak_factory_(this) {
  CupsProxyClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &CupsProxyServiceManager::OnDaemonAvailable, weak_factory_.GetWeakPtr()));
}

CupsProxyServiceManager::~CupsProxyServiceManager() = default;

void CupsProxyServiceManager::OnDaemonAvailable(bool daemon_available) {
  if (!daemon_available) {
    DVLOG(1) << "CupsProxyDaemon startup error";
    return;
  }

  // Attempt to start the service, which will then bootstrap a connection
  // with the daemon.
  // Note: The service does not support BindInterface calls, so we
  // intentionally leave out a connection_error_handler, since it would
  // called immediately.
  // TODO(crbug.com/945409): Manage our own service instance when ServiceManager
  // goes away.
  content::BrowserContext::GetConnectorFor(
      ProfileManager::GetPrimaryUserProfile())
      ->Connect(printing::mojom::kCupsProxyServiceName,
                service_handle_.BindNewPipeAndPassReceiver());
}

}  // namespace chromeos
