// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_
#define CONTENT_RENDERER_LOADER_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"

namespace content {

// WebURLLoaderFactory for InternetDisconnectedWebURLLoader.
class InternetDisconnectedWebURLLoaderFactory final
    : public blink::WebURLLoaderFactory {
 public:
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest&,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override;
};

// WebURLLoader which always returns an internet disconnected error. At present,
// this is used for ServiceWorker's offline-capability-check fetch event.
class InternetDisconnectedWebURLLoader final : public blink::WebURLLoader {
 public:
  explicit InternetDisconnectedWebURLLoader(
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle);
  ~InternetDisconnectedWebURLLoader() override;

  // WebURLLoader implementation:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<blink::WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool download_to_network_cache_only,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      blink::WebURLLoaderClient*,
      blink::WebURLResponse&,
      base::Optional<blink::WebURLError>&,
      blink::WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      blink::WebBlobInfo& downloaded_blob) override;
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<blink::WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool download_to_network_cache_only,
      bool no_mime_sniffing,
      blink::WebURLLoaderClient* client) override;
  void SetDefersLoading(bool defers) override;
  void DidChangePriority(blink::WebURLRequest::Priority, int) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;

 private:
  void DidFail(blink::WebURLLoaderClient* client,
               const blink::WebURLError& error);

  std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
      task_runner_handle_;
  base::WeakPtrFactory<InternetDisconnectedWebURLLoader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_INTERNET_DISCONNECTED_WEB_URL_LOADER_H_
