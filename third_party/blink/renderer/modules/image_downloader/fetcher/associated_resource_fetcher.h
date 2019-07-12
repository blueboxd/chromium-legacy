// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_FETCHER_ASSOCIATED_RESOURCE_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_FETCHER_ASSOCIATED_RESOURCE_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"

namespace blink {
class KURL;
class LocalFrame;
class WebAssociatedURLLoader;

// Interface to download resources asynchronously.
class AssociatedResourceFetcher {
  USING_FAST_MALLOC(AssociatedResourceFetcher);

 public:
  // This will be called asynchronously after the URL has been fetched,
  // successfully or not.  If there is a failure, response and data will both be
  // empty.  |response| and |data| are both valid until the URLFetcher instance
  // is destroyed.
  using StartCallback = base::OnceCallback<void(const WebURLResponse& response,
                                                const std::string& data)>;

  explicit AssociatedResourceFetcher(const KURL& url);
  ~AssociatedResourceFetcher();

  void SetSkipServiceWorker(bool skip_service_worker);
  void SetCacheMode(mojom::FetchCacheMode mode);

  // Associate the corresponding WebURLLoaderOptions to the loader. Must be
  // called before Start. Used if the LoaderType is FRAME_ASSOCIATED_LOADER.
  void SetLoaderOptions(const WebAssociatedURLLoaderOptions& options);

  // Starts the request using the specified frame.  Calls |callback| when
  // done.
  //
  // |fetch_request_mode| is the mode to use. See
  // https://fetch.spec.whatwg.org/#concept-request-mode.
  //
  // |fetch_credentials_mode| is the credentials mode to use. See
  // https://fetch.spec.whatwg.org/#concept-request-credentials-mode
  void Start(LocalFrame* frame,
             mojom::RequestContextType request_context,
             network::mojom::RequestMode request_mode,
             network::mojom::CredentialsMode credentials_mode,
             StartCallback callback);

  // Manually cancel the request.
  void Cancel();

 private:
  class ClientImpl;

  std::unique_ptr<WebAssociatedURLLoader> loader_;
  std::unique_ptr<ClientImpl> client_;

  // Options to send to the loader.
  WebAssociatedURLLoaderOptions options_;

  // Request to send.
  WebURLRequest request_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedResourceFetcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_FETCHER_ASSOCIATED_RESOURCE_FETCHER_H_
