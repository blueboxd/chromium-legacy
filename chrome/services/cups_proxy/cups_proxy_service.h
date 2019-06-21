// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_H_
#define CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_H_

#include <memory>
#include <string>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace chromeos {
namespace printing {

class CupsProxyServiceDelegate;

// CupsProxy Service Implementation.
//
// Singleton Chrome Service managed by the ServiceManager and lives in the
// browser process. Lazily initializes mojom::CupsProxier handler,
// |proxy_manager_|, and handles binding all incoming
// mojom::CupsProxierRequest's to it.
//
// Note: Service lifetime is the same as Profile; there will be future work to
// lazily intiate it on first use.
class CupsProxyService : public service_manager::Service {
 public:
  CupsProxyService(service_manager::mojom::ServiceRequest request,
                   std::unique_ptr<CupsProxyServiceDelegate> delegate);
  ~CupsProxyService() override;

 private:
  // service_manager::Service override.
  void OnStart() override;

  // This method is stubbed since the only expected consumer of this service is
  // a ChromeOS daemon; this connection is bootstrapped over D-Bus by the
  // below binding methods.
  void OnConnect(const service_manager::BindSourceInfo& source_info,
                 const std::string& interface_name,
                 mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Binds |proxy_manager| to a CupsProxierPtr and passes it to the
  // CupsProxyDaemon. The binding is accomplished via D-Bus bootstrap.
  void BindToCupsProxyDaemon();
  void OnBindToCupsProxyDaemon(const bool success);

  service_manager::ServiceBinding service_binding_;
  service_manager::BinderRegistry binder_registry_;

  // Delegate providing necessary Profile dependencies.
  std::unique_ptr<CupsProxyServiceDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CupsProxyService);
};

}  // namespace printing
}  // namespace chromeos

#endif  // CHROME_SERVICES_CUPS_PROXY_CUPS_PROXY_SERVICE_H_
