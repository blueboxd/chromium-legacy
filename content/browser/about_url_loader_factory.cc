// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/about_url_loader_factory.h"

#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

AboutURLLoaderFactory::AboutURLLoaderFactory() = default;
AboutURLLoaderFactory::~AboutURLLoaderFactory() = default;

void AboutURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  network::ResourceResponseHead response_head;
  response_head.mime_type = "text/html";
  client->OnReceiveResponse(response_head);

  // Create a data pipe for transmitting the empty response. The |producer|
  // doesn't add any data.
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  client->OnStartLoadingResponseBody(std::move(consumer));
  client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
}

void AboutURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader) {
  bindings_.AddBinding(this, std::move(loader));
}

}  // namespace content
