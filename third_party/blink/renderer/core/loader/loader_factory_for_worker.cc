// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/loader_factory_for_worker.h"

#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"

namespace blink {

void LoaderFactoryForWorker::Trace(Visitor* visitor) {
  visitor->Trace(global_scope_);
  LoaderFactory::Trace(visitor);
}

std::unique_ptr<WebURLLoader> LoaderFactoryForWorker::CreateURLLoader(
    const ResourceRequest& request,
    const ResourceLoaderOptions& options,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  WrappedResourceRequest wrapped(request);

  network::mojom::blink::URLLoaderFactoryPtr url_loader_factory;
  if (options.url_loader_factory) {
    options.url_loader_factory->data->Clone(MakeRequest(&url_loader_factory));
  }
  // Resolve any blob: URLs that haven't been resolved yet. The XHR and
  // fetch() API implementations resolve blob URLs earlier because there can
  // be arbitrarily long delays between creating requests with those APIs and
  // actually creating the URL loader here. Other subresource loading will
  // immediately create the URL loader so resolving those blob URLs here is
  // simplest.
  if (request.Url().ProtocolIs("blob") && !url_loader_factory) {
    global_scope_->GetPublicURLManager().Resolve(
        request.Url(), MakeRequest(&url_loader_factory));
  }

  if (url_loader_factory) {
    return web_context_
        ->WrapURLLoaderFactory(url_loader_factory.PassInterface().PassHandle())
        ->CreateURLLoader(wrapped, CreateTaskRunnerHandle(task_runner));
  }

  // Use |script_loader_factory_| to load types SCRIPT (classic imported
  // scripts) and SERVICE_WORKER (module main scripts and module imported
  // scripts). Note that classic main scripts are also SERVICE_WORKER but
  // loaded by the shadow page on the main thread, not here.
  if (request.GetRequestContext() == mojom::RequestContextType::SCRIPT ||
      request.GetRequestContext() ==
          mojom::RequestContextType::SERVICE_WORKER) {
    if (web_context_->GetScriptLoaderFactory()) {
      return web_context_->GetScriptLoaderFactory()->CreateURLLoader(
          wrapped, CreateTaskRunnerHandle(task_runner));
    }
  }

  return web_context_->GetURLLoaderFactory()->CreateURLLoader(
      wrapped, CreateTaskRunnerHandle(task_runner));
}

std::unique_ptr<CodeCacheLoader>
LoaderFactoryForWorker::CreateCodeCacheLoader() {
  return web_context_->CreateCodeCacheLoader();
}

// TODO(altimin): This is used when creating a URLLoader, and
// ResourceFetcher::GetTaskRunner is used whenever asynchronous tasks around
// resource loading are posted. Modify the code so that all the tasks related to
// loading a resource use the resource loader handle's task runner.
std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
LoaderFactoryForWorker::CreateTaskRunnerHandle(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return scheduler::WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
      std::move(task_runner));
}

}  // namespace blink
