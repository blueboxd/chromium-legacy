// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_url_loader_factory.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

namespace {

constexpr char kCrLf[] = "\r\n";

network::ResourceResponseHead CreateResourceResponse(
    const data_decoder::mojom::BundleResponsePtr& response) {
  DCHECK_EQ(net::HTTP_OK, response->response_code);

  std::vector<std::string> header_strings;
  header_strings.push_back("HTTP/1.1 ");
  header_strings.push_back(base::NumberToString(response->response_code));
  header_strings.push_back(" ");
  header_strings.push_back(net::GetHttpReasonPhrase(
      static_cast<net::HttpStatusCode>(response->response_code)));
  header_strings.push_back(kCrLf);
  for (const auto& it : response->response_headers) {
    header_strings.push_back(it.first);
    header_strings.push_back(": ");
    header_strings.push_back(it.second);
    header_strings.push_back(kCrLf);
  }
  header_strings.push_back(kCrLf);

  network::ResourceResponseHead response_head;

  response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(base::JoinString(header_strings, "")));
  response_head.headers->GetMimeTypeAndCharset(&response_head.mime_type,
                                               &response_head.charset);
  return response_head;
}

}  // namespace

// TODO(crbug.com/966753): Consider security models, i.e. plausible CORS
// adoption.
class BundledExchangesURLLoaderFactory::EntryLoader final
    : public network::mojom::URLLoader {
 public:
  EntryLoader(base::WeakPtr<BundledExchangesURLLoaderFactory> factory,
              mojo::PendingRemote<network::mojom::URLLoaderClient> client,
              const GURL& url)
      : factory_(std::move(factory)), loader_client_(std::move(client)) {
    DCHECK(factory_);
    factory_->GetReader()->ReadResponse(
        url, base::BindOnce(&EntryLoader::OnResponseReady,
                            weak_factory_.GetWeakPtr()));
  }
  ~EntryLoader() override = default;

 private:
  // network::mojom::URLLoader implementation
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  void OnResponseReady(data_decoder::mojom::BundleResponsePtr response) {
    if (!factory_ || !loader_client_.is_connected())
      return;

    // TODO(crbug.com/990733): For the initial implementation, we allow only
    // net::HTTP_OK, but we should clarify acceptable status code in the spec.
    if (!response || response->response_code != net::HTTP_OK) {
      loader_client_->OnComplete(network::URLLoaderCompletionStatus(
          net::ERR_INVALID_BUNDLED_EXCHANGES));
      return;
    }

    loader_client_->OnReceiveResponse(CreateResourceResponse(response));

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes =
        std::min(static_cast<uint64_t>(network::kDataPipeDefaultAllocationSize),
                 response->payload_length);

    auto result =
        mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle);
    loader_client_->OnStartLoadingResponseBody(std::move(consumer_handle));
    if (result != MOJO_RESULT_OK) {
      loader_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }

    factory_->GetReader()->ReadResponseBody(
        std::move(response), std::move(producer_handle),
        base::BindOnce(&EntryLoader::FinishReadingBody,
                       weak_factory_.GetWeakPtr()));
  }

  void FinishReadingBody(MojoResult result) {
    if (!loader_client_.is_connected())
      return;

    network::URLLoaderCompletionStatus status;
    status.error_code =
        result == MOJO_RESULT_OK ? net::OK : net::ERR_UNEXPECTED;
    loader_client_->OnComplete(status);
  }

  base::WeakPtr<BundledExchangesURLLoaderFactory> factory_;
  mojo::Remote<network::mojom::URLLoaderClient> loader_client_;

  base::WeakPtrFactory<EntryLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EntryLoader);
};

BundledExchangesURLLoaderFactory::BundledExchangesURLLoaderFactory(
    std::unique_ptr<BundledExchangesReader> reader)
    : reader_(std::move(reader)) {}

BundledExchangesURLLoaderFactory::~BundledExchangesURLLoaderFactory() = default;

void BundledExchangesURLLoaderFactory::SetFallbackFactory(
    mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory) {
  fallback_factory_ = std::move(fallback_factory);
}

void BundledExchangesURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr loader_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (base::EqualsCaseInsensitiveASCII(resource_request.method,
                                       net::HttpRequestHeaders::kGetMethod) &&
      reader_->HasEntry(resource_request.url)) {
    auto loader = std::make_unique<EntryLoader>(weak_factory_.GetWeakPtr(),
                                                loader_client.PassInterface(),
                                                resource_request.url);
    std::unique_ptr<network::mojom::URLLoader> url_loader = std::move(loader);
    mojo::MakeSelfOwnedReceiver(
        std::move(url_loader), mojo::PendingReceiver<network::mojom::URLLoader>(
                                   std::move(loader_request)));
  } else if (fallback_factory_) {
    fallback_factory_->CreateLoaderAndStart(
        std::move(loader_request), routing_id, request_id, options,
        resource_request, std::move(loader_client), traffic_annotation);
  } else {
    loader_client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_FAILED));
  }
}

void BundledExchangesURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace content
