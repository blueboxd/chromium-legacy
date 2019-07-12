// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_utils.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/transferrable_url_loader.mojom.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/system/data_pipe.h"

PluginResponseInterceptorURLLoaderThrottle::
    PluginResponseInterceptorURLLoaderThrottle(
        content::ResourceContext* resource_context,
        int resource_type,
        int frame_tree_node_id)
    : resource_context_(resource_context),
      resource_type_(resource_type),
      frame_tree_node_id_(frame_tree_node_id) {}

PluginResponseInterceptorURLLoaderThrottle::
    PluginResponseInterceptorURLLoaderThrottle(
        content::BrowserContext* browser_context,
        int resource_type,
        int frame_tree_node_id)
    : browser_context_(browser_context),
      resource_type_(resource_type),
      frame_tree_node_id_(frame_tree_node_id) {}

PluginResponseInterceptorURLLoaderThrottle::
    ~PluginResponseInterceptorURLLoaderThrottle() = default;

void PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::ResourceResponseHead* response_head,
    bool* defer) {
  if (content::download_utils::MustDownload(response_url,
                                            response_head->headers.get(),
                                            response_head->mime_type)) {
    return;
  }

  std::string extension_id;
  if (resource_context_) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    extension_id = PluginUtils::GetExtensionIdForMimeType(
        resource_context_, response_head->mime_type);
  } else {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    extension_id = PluginUtils::GetExtensionIdForMimeType(
        browser_context_, response_head->mime_type);
  }

  if (extension_id.empty())
    return;

  std::string view_id = base::GenerateGUID();
  // The string passed down to the original client with the response body.
  std::string payload = view_id;

  network::mojom::URLLoaderPtr dummy_new_loader;
  mojo::MakeRequest(&dummy_new_loader);
  network::mojom::URLLoaderClientPtr new_client;
  network::mojom::URLLoaderClientRequest new_client_request =
      mojo::MakeRequest(&new_client);

  uint32_t data_pipe_size = 64U;
  // Provide the MimeHandlerView code a chance to override the payload. This is
  // the case where the resource is handled by frame-based MimeHandlerView.
  *defer = extensions::MimeHandlerViewAttachHelper::
      OverrideBodyForInterceptedResponse(
          frame_tree_node_id_, response_url, response_head->mime_type, view_id,
          &payload, &data_pipe_size,
          base::BindOnce(
              &PluginResponseInterceptorURLLoaderThrottle::ResumeLoad,
              weak_factory_.GetWeakPtr()));

  mojo::DataPipe data_pipe(data_pipe_size);
  uint32_t len = static_cast<uint32_t>(payload.size());
  CHECK_EQ(MOJO_RESULT_OK,
           data_pipe.producer_handle->WriteData(
               payload.c_str(), &len, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));

  new_client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));

  network::URLLoaderCompletionStatus status(net::OK);
  status.decoded_body_length = len;
  new_client->OnComplete(status);

  network::mojom::URLLoaderPtr original_loader;
  network::mojom::URLLoaderClientRequest original_client;
  delegate_->InterceptResponse(std::move(dummy_new_loader),
                               std::move(new_client_request), &original_loader,
                               &original_client);

  // Make a deep copy of ResourceResponseHead before passing it cross-thread.
  auto resource_response = base::MakeRefCounted<network::ResourceResponse>();
  resource_response->head = *response_head;
  auto deep_copied_response = resource_response->DeepCopy();

  auto transferrable_loader = content::mojom::TransferrableURLLoader::New();
  transferrable_loader->url = GURL(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id).spec() +
      base::GenerateGUID());
  transferrable_loader->url_loader = original_loader.PassInterface();
  transferrable_loader->url_loader_client = std::move(original_client);
  transferrable_loader->head = std::move(deep_copied_response->head);
  transferrable_loader->head.intercepted_by_plugin = true;

  bool embedded =
      resource_type_ != static_cast<int>(content::ResourceType::kMainFrame);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &extensions::StreamsPrivateAPI::SendExecuteMimeTypeHandlerEvent,
          extension_id, view_id, embedded, frame_tree_node_id_,
          -1 /* render_process_id */, -1 /* render_frame_id */,
          std::move(transferrable_loader), response_url));
}

void PluginResponseInterceptorURLLoaderThrottle::ResumeLoad() {
  delegate_->Resume();
}
