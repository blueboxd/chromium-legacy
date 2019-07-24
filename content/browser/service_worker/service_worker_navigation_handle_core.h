// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_CORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_CORE_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandle;

// This class is created on the UI thread, but should only be accessed from the
// IO thread afterwards. It is the IO thread pendant of
// ServiceWorkerNavigationHandle. See the ServiceWorkerNavigationHandle header
// for more details about the lifetime of both classes.
class CONTENT_EXPORT ServiceWorkerNavigationHandleCore {
 public:
  ServiceWorkerNavigationHandleCore(
      base::WeakPtr<ServiceWorkerNavigationHandle> ui_handle,
      ServiceWorkerContextWrapper* context_wrapper);
  ~ServiceWorkerNavigationHandleCore();

  // Called when a ServiceWorkerProviderHost was created.
  void OnCreatedProviderHost(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info);

  // Called when the navigation is ready to commit, set the 2 IDs for the
  // pre-created provider host.
  void OnBeginNavigationCommit(int render_process_id, int render_frame_id);

  void OnBeginWorkerCommit();

  ServiceWorkerContextWrapper* context_wrapper() const {
    return context_wrapper_.get();
  }

  /////////////////////////////////////////////////////////////////////////////
  // NavigationLoaderOnUI:

  void set_provider_host(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host) {
    provider_host_ = std::move(provider_host);
  }

  base::WeakPtr<ServiceWorkerProviderHost> provider_host() {
    return provider_host_;
  }

  void set_interceptor(
      std::unique_ptr<ServiceWorkerControlleeRequestHandler> interceptor) {
    interceptor_ = std::move(interceptor);
  }

  ServiceWorkerControlleeRequestHandler* interceptor() {
    return interceptor_.get();
  }

  base::WeakPtr<ServiceWorkerNavigationHandleCore> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  /////////////////////////////////////////////////////////////////////////////

 private:
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;
  base::WeakPtr<ServiceWorkerNavigationHandle> ui_handle_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;

  // NavigationLoaderOnUI:
  std::unique_ptr<ServiceWorkerControlleeRequestHandler> interceptor_;

  base::WeakPtrFactory<ServiceWorkerNavigationHandleCore> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerNavigationHandleCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_NAVIGATION_HANDLE_CORE_H_
