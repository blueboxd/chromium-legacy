// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_url_loader_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/blob_internals_url_loader.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/webui/network_error_url_loader.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "ui/base/template_expressions.h"

namespace content {

namespace {

class WebUIURLLoaderFactory;

void CallOnError(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    int error_code) {
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));

  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  client->OnComplete(status);
}

void ReadData(
    network::mojom::URLResponseHeadPtr headers,
    const ui::TemplateReplacements* replacements,
    bool replace_in_js,
    scoped_refptr<URLDataSourceImpl> data_source,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    scoped_refptr<base::RefCountedMemory> bytes) {
  if (!bytes) {
    CallOnError(std::move(client_remote), net::ERR_FAILED);
    return;
  }

  if (replacements) {
    // We won't know the the final output size ahead of time, so we have to
    // use an intermediate string.
    base::StringPiece input(reinterpret_cast<const char*>(bytes->front()),
                            bytes->size());
    std::string temp_str;
    if (replace_in_js) {
      CHECK(
          ui::ReplaceTemplateExpressionsInJS(input, *replacements, &temp_str));
    } else {
      temp_str = ui::ReplaceTemplateExpressions(input, *replacements);
    }
    bytes = base::RefCountedString::TakeString(&temp_str);
  }

  uint32_t output_size = bytes->size();

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = output_size;
  mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
  MojoResult create_result = mojo::CreateDataPipe(
      &options, &pipe_producer_handle, &pipe_consumer_handle);
  CHECK_EQ(create_result, MOJO_RESULT_OK);

  void* buffer = nullptr;
  uint32_t num_bytes = output_size;
  MojoResult result = pipe_producer_handle->BeginWriteData(
      &buffer, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  CHECK_EQ(result, MOJO_RESULT_OK);
  CHECK_GE(num_bytes, output_size);

  memcpy(buffer, bytes->front(), output_size);
  result = pipe_producer_handle->EndWriteData(output_size);
  CHECK_EQ(result, MOJO_RESULT_OK);

  // For media content, |content_length| must be known upfront for data that is
  // assumed to be fully buffered (as opposed to streamed from the network),
  // otherwise the media player will get confused and refuse to play.
  // Content delivered via chrome:// URLs is assumed fully buffered.
  headers->content_length = output_size;

  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));
  client->OnReceiveResponse(std::move(headers));

  client->OnStartLoadingResponseBody(std::move(pipe_consumer_handle));
  network::URLLoaderCompletionStatus status(net::OK);
  status.encoded_data_length = output_size;
  status.encoded_body_length = output_size;
  status.decoded_body_length = output_size;
  client->OnComplete(status);
}

void DataAvailable(
    network::mojom::URLResponseHeadPtr headers,
    const ui::TemplateReplacements* replacements,
    bool replace_in_js,
    scoped_refptr<URLDataSourceImpl> source,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    scoped_refptr<base::RefCountedMemory> bytes) {
  // Since the bytes are from the memory mapped resource file, copying the
  // data can lead to disk access. Needs to be posted to a SequencedTaskRunner
  // as Mojo requires a SequencedTaskRunnerHandle in scope.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTask(FROM_HERE, base::BindOnce(ReadData, std::move(headers),
                                           replacements, replace_in_js, source,
                                           std::move(client_remote), bytes));
}

void StartURLLoader(
    const network::ResourceRequest& request,
    int frame_tree_node_id,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    BrowserContext* browser_context) {
  // NOTE: this duplicates code in URLDataManagerBackend::StartRequest.
  if (!URLDataManagerBackend::CheckURLIsValid(request.url)) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  URLDataSourceImpl* source =
      URLDataManagerBackend::GetForBrowserContext(browser_context)
          ->GetDataSourceFromURL(request.url);
  if (!source) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  if (!source->source()->ShouldServiceRequest(request.url, browser_context,
                                              -1)) {
    CallOnError(std::move(client_remote), net::ERR_INVALID_URL);
    return;
  }

  std::string path = URLDataSource::URLToRequestPath(request.url);
  std::string origin_header;
  request.headers.GetHeader(net::HttpRequestHeaders::kOrigin, &origin_header);

  scoped_refptr<net::HttpResponseHeaders> headers =
      URLDataManagerBackend::GetHeaders(source, path, origin_header);

  auto resource_response = network::mojom::URLResponseHead::New();

  resource_response->headers = headers;
  // Headers from WebUI are trusted, so parsing can happen from a non-sandboxed
  // process.
  resource_response->parsed_headers =
      network::PopulateParsedHeaders(resource_response->headers, request.url);
  resource_response->mime_type = source->source()->GetMimeType(path);
  // TODO: fill all the time related field i.e. request_time response_time
  // request_start response_start

  WebContents::Getter wc_getter =
      base::BindRepeating(WebContents::FromFrameTreeNodeId, frame_tree_node_id);

  bool replace_in_js =
      source->source()->ShouldReplaceI18nInJS() &&
      source->source()->GetMimeType(path) == "application/javascript";

  const ui::TemplateReplacements* replacements = nullptr;
  if (source->source()->GetMimeType(path) == "text/html" || replace_in_js)
    replacements = source->source()->GetReplacements();

  // To keep the same behavior as the old WebUI code, we call the source to get
  // the value for |replacements| on the IO thread. Since |replacements| is
  // owned by |source| keep a reference to it in the callback.
  URLDataSource::GotDataCallback data_available_callback = base::BindOnce(
      DataAvailable, std::move(resource_response), replacements, replace_in_js,
      base::RetainedRef(source), std::move(client_remote));

  source->source()->StartDataRequest(request.url, std::move(wc_getter),
                                     std::move(data_available_callback));
}

// This class has two ownership models. When it's created by
// CreateWebUIURLLoaderBinding it is owned by its receivers and will delete
// itself when it has no more receivers. Otherwise it's strongly owned.
class WebUIURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  // |allowed_hosts| is an optional set of allowed host names. If empty then
  // all hosts are allowed.
  WebUIURLLoaderFactory(
      FrameTreeNode* ftn,
      const std::string& scheme,
      base::flat_set<std::string> allowed_hosts,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver =
          mojo::PendingReceiver<network::mojom::URLLoaderFactory>())
      : frame_tree_node_id_(ftn->frame_tree_node_id()),
        scheme_(scheme),
        allowed_hosts_(std::move(allowed_hosts)) {
    if (factory_receiver) {
      loader_factory_receivers_.set_disconnect_handler(base::BindRepeating(
          &WebUIURLLoaderFactory::OnDisconnect, base::Unretained(this)));
      loader_factory_receivers_.Add(this, std::move(factory_receiver));
    }
  }

  ~WebUIURLLoaderFactory() override {}

  // network::mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    auto* ftn = FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
    if (!ftn) {
      CallOnError(std::move(client), net::ERR_FAILED);
      return;
    }

    BrowserContext* browser_context =
        ftn->current_frame_host()->GetBrowserContext();

    if (request.url.scheme() != scheme_) {
      DVLOG(1) << "Bad scheme: " << request.url.scheme();
      if (loader_factory_receivers_.empty()) {
        // This factory is being used directly without going through a mojo pipe
        // so just assert.
        CHECK(false) << "Incorrect scheme";
      } else {
        loader_factory_receivers_.ReportBadMessage("Incorrect scheme");
      }
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    if (!allowed_hosts_.empty() &&
        (!request.url.has_host() ||
         allowed_hosts_.find(request.url.host()) == allowed_hosts_.end())) {
      // Temporary reporting the bad WebUI host for for http://crbug.com/837328.
      static auto* crash_key = base::debug::AllocateCrashKeyString(
          "webui_url", base::debug::CrashKeySize::Size64);
      base::debug::SetCrashKeyString(crash_key, request.url.spec());

      DVLOG(1) << "Bad host: \"" << request.url.host() << '"';
      if (loader_factory_receivers_.empty()) {
        CHECK(false) << "Incorrect host";
      } else {
        loader_factory_receivers_.ReportBadMessage("Incorrect host");
      }
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    if (request.url.host_piece() == kChromeUIBlobInternalsHost) {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &StartBlobInternalsURLLoader, request, std::move(client),
              base::Unretained(
                  ChromeBlobStorageContext::GetFor(browser_context))));
      return;
    }

    if (request.url.host_piece() == kChromeUINetworkErrorHost ||
        request.url.host_piece() == kChromeUIDinoHost) {
      StartNetworkErrorsURLLoader(request, std::move(client));
      return;
    }

    // We pass the FrameTreeNode ID to get to the WebContents because requests
    // from frames can happen while the RFH is changed for a cross-process
    // navigation. The URLDataSources just need the WebContents; the specific
    // frame doesn't matter.
    StartURLLoader(request, frame_tree_node_id_, std::move(client),
                   browser_context);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    loader_factory_receivers_.Add(this, std::move(receiver));
  }

  const std::string& scheme() const { return scheme_; }

 private:
  void OnDisconnect() {
    if (loader_factory_receivers_.empty())
      delete this;
  }

  int const frame_tree_node_id_;
  const std::string scheme_;
  const base::flat_set<std::string> allowed_hosts_;  // if empty all allowed.
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> loader_factory_receivers_;

  DISALLOW_COPY_AND_ASSIGN(WebUIURLLoaderFactory);
};

}  // namespace

std::unique_ptr<network::mojom::URLLoaderFactory> CreateWebUIURLLoader(
    RenderFrameHost* render_frame_host,
    const std::string& scheme,
    base::flat_set<std::string> allowed_hosts) {
  return std::make_unique<WebUIURLLoaderFactory>(
      FrameTreeNode::From(render_frame_host), scheme, std::move(allowed_hosts));
}

void CreateWebUIURLLoaderBinding(
    FrameTreeNode* node,
    const std::string& scheme,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver) {
  // Deletes itself when the last receiver is destructed.
  new WebUIURLLoaderFactory(node, scheme, base::flat_set<std::string>(),
                            std::move(factory_receiver));
}

}  // namespace content
