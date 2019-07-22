// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/data_url_loader_factory.h"

#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/url_request/url_request_data_job.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

namespace {
struct WriteData {
  network::mojom::URLLoaderClientPtr client;
  std::string data;
  std::unique_ptr<mojo::DataPipeProducer> producer;
};

void OnWrite(std::unique_ptr<WriteData> write_data, MojoResult result) {
  if (result != MOJO_RESULT_OK) {
    network::URLLoaderCompletionStatus status(net::ERR_FAILED);
    return;
  }

  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = write_data->data.size();
  status.encoded_body_length = write_data->data.size();
  status.decoded_body_length = write_data->data.size();
  write_data->client->OnComplete(status);
}

}  // namespace

DataURLLoaderFactory::DataURLLoaderFactory() = default;
DataURLLoaderFactory::~DataURLLoaderFactory() = default;

DataURLLoaderFactory::DataURLLoaderFactory(const GURL& url) : url_(url) {}

void DataURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  std::string mime_type;
  std::string charset;
  std::string data;
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(std::string());

  const GURL* url = nullptr;
  if (!url_.is_empty() && request.url.is_empty()) {
    url = &url_;
  } else {
    url = &request.url;
  }

  int result = net::URLRequestDataJob::BuildResponse(*url, &mime_type, &charset,
                                                     &data, headers.get());
  url_ = GURL();  // Don't need it anymore.

  if (result != net::OK) {
    client->OnComplete(network::URLLoaderCompletionStatus(result));
    return;
  }

  network::ResourceResponseHead response;
  response.mime_type = mime_type;
  response.charset = charset;
  response.headers = headers;
  client->OnReceiveResponse(response);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, &producer, &consumer) != MOJO_RESULT_OK) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  client->OnStartLoadingResponseBody(std::move(consumer));

  auto write_data = std::make_unique<WriteData>();
  write_data->client = std::move(client);
  write_data->data = std::move(data);
  write_data->producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer));

  base::StringPiece string_piece(write_data->data);

  write_data->producer->Write(
      std::make_unique<mojo::StringDataSource>(
          string_piece, mojo::StringDataSource::AsyncWritingMode::
                            STRING_STAYS_VALID_UNTIL_COMPLETION),
      base::BindOnce(OnWrite, std::move(write_data)));
}

void DataURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader) {
  bindings_.AddBinding(this, std::move(loader));
}

}  // namespace content
