// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PDF_COMPOSITOR_PDF_COMPOSITOR_SERVICE_H_
#define COMPONENTS_SERVICES_PDF_COMPOSITOR_PDF_COMPOSITOR_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/services/pdf_compositor/public/mojom/pdf_compositor.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/service_keepalive.h"

namespace printing {

class PdfCompositorService : public service_manager::Service {
 public:
  explicit PdfCompositorService(service_manager::mojom::ServiceRequest request);
  ~PdfCompositorService() override;

  // Factory function for use as an embedded service.
  static std::unique_ptr<service_manager::Service> Create(
      service_manager::mojom::ServiceRequest request);

  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void set_skip_initialization_for_testing(bool skip) {
    skip_initialization_for_testing_ = skip;
  }

 private:
  service_manager::ServiceBinding binding_;
  service_manager::ServiceKeepalive keepalive_;
  bool skip_initialization_for_testing_ = false;
  std::unique_ptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
  service_manager::BinderRegistry registry_;
  base::WeakPtrFactory<PdfCompositorService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PdfCompositorService);
};

}  // namespace printing

#endif  // COMPONENTS_SERVICES_PDF_COMPOSITOR_PDF_COMPOSITOR_SERVICE_H_
