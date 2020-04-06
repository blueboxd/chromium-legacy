// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_failing_url_loader_factory.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

// A WebURLLoader which always fails loading.
class FailingLoader final : public WebURLLoader {
 public:
  explicit FailingLoader(
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle)
      : task_runner_handle_(std::move(task_runner_handle)) {}
  ~FailingLoader() override = default;

  // WebURLLoader implementation:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool download_to_network_cache_only,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient*,
      WebURLResponse&,
      base::Optional<WebURLError>& error,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob) override {
    error = WebURLError(ResourceError::Failure(KURL(request->url)));
  }
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequest::ExtraData> request_extra_data,
      int requestor_id,
      bool download_to_network_cache_only,
      bool no_mime_sniffing,
      WebURLLoaderClient* client) override {
    GetTaskRunner()->PostTask(
        FROM_HERE, WTF::Bind(&FailingLoader::Fail, weak_factory_.GetWeakPtr(),
                             KURL(request->url), WTF::Unretained(client)));
  }
  void SetDefersLoading(bool) override {}
  void DidChangePriority(WebURLRequest::Priority, int) override {}
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return task_runner_handle_->GetTaskRunner();
  }

 private:
  void Fail(const KURL& url, WebURLLoaderClient* client) {
    client->DidFail(WebURLError(ResourceError::Failure(url)), 0, 0, 0);
  }

  const std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
      task_runner_handle_;

  // This must be the last member.
  base::WeakPtrFactory<FailingLoader> weak_factory_{this};
};

}  // namespace

std::unique_ptr<WebURLLoader> WebFailingURLLoaderFactory::CreateURLLoader(
    const WebURLRequest&,
    std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle> handle) {
  return std::make_unique<FailingLoader>(std::move(handle));
}

}  // namespace blink
