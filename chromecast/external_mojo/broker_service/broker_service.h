// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_BROKER_SERVICE_BROKER_SERVICE_H_
#define CHROMECAST_EXTERNAL_MOJO_BROKER_SERVICE_BROKER_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequence_bound.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace base {
class SequencedTaskRunner;
class Thread;
}  // namespace base

namespace chromecast {
namespace external_mojo {
class ExternalMojoBroker;

// A Mojo service (intended to run within cast_shell or some other Chromium
// ServiceManager environment) that allows Mojo services built into external
// processes to interoperate with the Mojo services within cast_shell.
class BrokerService : public ::service_manager::Service {
 public:
  static constexpr char const* kServiceName = "external_mojo_broker";

  // Adds a manifest for an external Mojo service (ie, one that is running in
  // a non-Chromium process). A manifest is only needed for external services
  // that bind to Mojo services within cast_shell, or for external services that
  // are bound to (used) by internal Mojo services. All external manifests must
  // be added before GetExternalMojoBrokerManifest() is called (otherwise they
  // will not be included in the broker manifest, and so the relevant
  // permissions will not be set correctly).
  static void AddExternalServiceManifest(service_manager::Manifest manifest);

  // Returns the manifest for this service.
  static const service_manager::Manifest& GetManifest();

  explicit BrokerService(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);
  ~BrokerService() override;

 private:
  service_manager::ServiceReceiver service_receiver_;

  std::unique_ptr<base::Thread> io_thread_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  base::SequenceBound<ExternalMojoBroker> broker_;

  DISALLOW_COPY_AND_ASSIGN(BrokerService);
};

}  // namespace external_mojo
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_BROKER_SERVICE_BROKER_SERVICE_H_
