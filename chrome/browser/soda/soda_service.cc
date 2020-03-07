// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/soda/soda_service.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/service_process_host.h"

namespace soda {

constexpr base::TimeDelta kIdleProcessTimeout = base::TimeDelta::FromSeconds(5);

SodaService::SodaService() = default;
SodaService::~SodaService() = default;

void SodaService::Create(
    mojo::PendingReceiver<media::mojom::SodaContext> receiver) {
  LaunchIfNotRunning();
  soda_service_->BindContext(std::move(receiver));
}

void SodaService::LaunchIfNotRunning() {
  if (soda_service_.is_bound())
    return;

  content::ServiceProcessHost::Launch(
      soda_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_SODA_SERVICE_NAME)
          .WithSandboxType(service_manager::SandboxType::kSoda)
          .Pass());

  // Ensure that if the interface is ever disconnected (e.g. the service
  // process crashes) or goes idle for a short period of time -- meaning there
  // are no in-flight messages and no other interfaces bound through this
  // one -- then we will reset |remote|, causing the service process to be
  // terminated if it isn't already.
  soda_service_.reset_on_disconnect();
  soda_service_.reset_on_idle_timeout(kIdleProcessTimeout);
}

}  // namespace soda
