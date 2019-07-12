// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/image_downloader/fetcher/associated_resource_fetcher.h"

#include <stdint.h>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/web_associated_url_loader_impl.h"
#include "third_party/blink/renderer/modules/image_downloader/fetcher/associated_resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class AssociatedResourceFetcher::ClientImpl
    : public WebAssociatedURLLoaderClient {
  USING_FAST_MALLOC(AssociatedResourceFetcher::ClientImpl);

 public:
  explicit ClientImpl(StartCallback callback)
      : completed_(false), status_(LOADING), callback_(std::move(callback)) {}

  ~ClientImpl() override {}

  virtual void Cancel() { OnLoadCompleteInternal(LOAD_FAILED); }

  bool completed() const { return completed_; }

 private:
  enum LoadStatus {
    LOADING,
    LOAD_FAILED,
    LOAD_SUCCEEDED,
  };

  void OnLoadCompleteInternal(LoadStatus status) {
    DCHECK(!completed_);
    DCHECK_EQ(status_, LOADING);

    completed_ = true;
    status_ = status;

    if (callback_.is_null())
      return;
    std::move(callback_).Run(
        status_ == LOAD_FAILED ? WebURLResponse() : response_,
        status_ == LOAD_FAILED ? std::string() : data_);
  }

  // WebAssociatedURLLoaderClient methods:
  void DidReceiveResponse(const WebURLResponse& response) override {
    DCHECK(!completed_);
    response_ = response;
  }
  void DidReceiveCachedMetadata(const char* data, int data_length) override {
    DCHECK(!completed_);
    DCHECK_GT(data_length, 0);
  }
  void DidReceiveData(const char* data, int data_length) override {
    // The WebAssociatedURLLoader will continue after a load failure.
    // For example, for an Access Control error.
    if (completed_)
      return;
    DCHECK_GT(data_length, 0);

    data_.append(data, data_length);
  }
  void DidFinishLoading() override {
    // The WebAssociatedURLLoader will continue after a load failure.
    // For example, for an Access Control error.
    if (completed_)
      return;
    OnLoadCompleteInternal(LOAD_SUCCEEDED);
  }
  void DidFail(const WebURLError& error) override {
    OnLoadCompleteInternal(LOAD_FAILED);
  }

 private:
  // Set to true once the request is complete.
  bool completed_;

  // Buffer to hold the content from the server.
  std::string data_;

  // A copy of the original resource response.
  WebURLResponse response_;

  LoadStatus status_;

  // Callback when we're done.
  StartCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ClientImpl);
};

AssociatedResourceFetcher::AssociatedResourceFetcher(const KURL& url)
    : request_(url) {}

AssociatedResourceFetcher::~AssociatedResourceFetcher() {
  if (!loader_)
    return;

  DCHECK(client_);

  if (!client_->completed())
    loader_->Cancel();
}

void AssociatedResourceFetcher::SetSkipServiceWorker(bool skip_service_worker) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  request_.SetSkipServiceWorker(skip_service_worker);
}

void AssociatedResourceFetcher::SetCacheMode(mojom::FetchCacheMode mode) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  request_.SetCacheMode(mode);
}

void AssociatedResourceFetcher::SetLoaderOptions(
    const WebAssociatedURLLoaderOptions& options) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  options_ = options;
}

void AssociatedResourceFetcher::Start(
    LocalFrame* frame,
    mojom::RequestContextType request_context,
    network::mojom::RequestMode request_mode,
    network::mojom::CredentialsMode credentials_mode,
    StartCallback callback) {
  DCHECK(!loader_);
  DCHECK(!client_);
  DCHECK(!request_.IsNull());
  if (!request_.HttpBody().IsNull())
    DCHECK_NE("GET", request_.HttpMethod().Utf8()) << "GETs can't have bodies.";

  request_.SetRequestContext(request_context);
  request_.SetSiteForCookies(frame->GetDocument()->SiteForCookies());
  request_.SetMode(request_mode);
  request_.SetCredentialsMode(credentials_mode);

  client_ = std::make_unique<ClientImpl>(std::move(callback));

  loader_ = std::make_unique<WebAssociatedURLLoaderImpl>(frame->GetDocument(),
                                                         options_);
  loader_->LoadAsynchronously(request_, client_.get());

  // No need to hold on to the request; reset it now.
  request_ = WebURLRequest();
}

void AssociatedResourceFetcher::Cancel() {
  loader_->Cancel();
  client_->Cancel();
}

}  // namespace blink
