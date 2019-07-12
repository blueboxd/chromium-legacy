// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/previews/previews_lite_page_serving_url_loader.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace previews {

using HandleRequest = base::OnceCallback<void(
    std::unique_ptr<PreviewsLitePageServingURLLoader> serving_url_loader,
    content::URLLoaderRequestInterceptor::RequestHandler handler)>;

// A URL loader that attempts to fetch an HTTPS server lite page, and if
// successful, redirects to the lite page URL, and hands the underlying
// network URLLoader to a success callback. Currently, it supports serving the
// Preview and falling back to default behavior.
class PreviewsLitePageRedirectURLLoader : public network::mojom::URLLoader {
 public:
  PreviewsLitePageRedirectURLLoader(
      const network::ResourceRequest& tentative_resource_request,
      HandleRequest callback);
  ~PreviewsLitePageRedirectURLLoader() override;

  // Creates and starts |serving_url_loader_|. |chrome_proxy_headers| are added
  // to the request, and the other parameters are used to start the network
  // service URLLoader.
  void StartRedirectToPreview(
      const net::HttpRequestHeaders& chrome_proxy_headers,
      const scoped_refptr<network::SharedURLLoaderFactory>&
          network_loader_factory,
      int frame_tree_node_id);

  // Creates a redirect to |original_url|.
  void StartRedirectToOriginalURL(const GURL& original_url);

 private:
  // network::mojom::URLLoader:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Processes |result|. Used as a callback for |serving_url_loader_|.
  // |redirect_info| and |response| should be present if and only if |result| is
  // kRedirect.
  void OnResultDetermined(ServingLoaderResult result,
                          base::Optional<net::RedirectInfo> redirect_info,
                          scoped_refptr<network::ResourceResponse> response);

  // Called when the lite page can be successfully served.
  void OnLitePageSuccess();

  // Called when a non-200, non-307 response is received from the previews
  // server.
  void OnLitePageFallback();

  // Called when a redirect (307) is received from the previews server.
  void OnLitePageRedirect(const net::RedirectInfo& redirect_info,
                          const network::ResourceResponseHead& response_head);

  // The handler when trying to serve the lite page to the user. Serves a
  // redirect to the lite page server URL.
  void StartHandlingRedirectToModifiedRequest(
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderRequest request,
      network::mojom::URLLoaderClientPtr client);

  // Helper method for setting up and serving |redirect_info| to |client|.
  void StartHandlingRedirect(
      const network::ResourceRequest& /* resource_request */,
      network::mojom::URLLoaderRequest request,
      network::mojom::URLLoaderClientPtr client);

  // Helper method to create redirect information to |redirect_url| and modify
  // |redirect_info_| and |modified_resource_request_|.
  void CreateRedirectInformation(const GURL& redirect_url);

  // Mojo error handling. Deletes |this|.
  void OnConnectionClosed();

  // The underlying URLLoader that speculatively tries to fetch the lite page.
  std::unique_ptr<PreviewsLitePageServingURLLoader> serving_url_loader_;

  // A copy of the initial resource request that has been modified to fetch
  // the lite page.
  network::ResourceRequest modified_resource_request_;

  // Stores the response when a 307 (redirect) is received from the previews
  // server.
  network::ResourceResponseHead response_head_;

  // Information about the redirect to the lite page server.
  net::RedirectInfo redirect_info_;

  // Called upon success or failure to let content/ know whether this class
  // intends to intercept the request. Must be passed a handler if this class
  // intends to intercept the request.
  HandleRequest callback_;

  // Binding to the URLLoader interface.
  mojo::Binding<network::mojom::URLLoader> binding_;

  // The owning client. Used for serving redirects.
  network::mojom::URLLoaderClientPtr client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreviewsLitePageRedirectURLLoader> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageRedirectURLLoader);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_REDIRECT_URL_LOADER_H_
