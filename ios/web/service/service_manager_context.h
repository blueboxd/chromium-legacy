// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SERVICE_SERVICE_MANAGER_CONTEXT_H_
#define IOS_WEB_SERVICE_SERVICE_MANAGER_CONTEXT_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace web {

// ServiceManagerContext manages the browser's connection to the ServiceManager,
// hosting an in-process ServiceManagerContext.
class ServiceManagerContext {
 public:
  ServiceManagerContext();
  ~ServiceManagerContext();

 private:
  class InProcessServiceManagerContext;

  void RunService(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);
  void OnServiceQuit(service_manager::Service* service);

  scoped_refptr<InProcessServiceManagerContext> in_process_context_;
  std::map<service_manager::Service*, std::unique_ptr<service_manager::Service>>
      running_services_;
  base::WeakPtrFactory<ServiceManagerContext> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerContext);
};

}  // namespace web

#endif  // IOS_WEB_SERVICE_SERVICE_MANAGER_CONTEXT_H_
