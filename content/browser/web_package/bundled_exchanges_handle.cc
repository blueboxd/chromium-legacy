// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_handle.h"

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "content/browser/web_package/bundled_exchanges_url_loader_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

namespace {

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("bundled_exchanges_start_url_loader",
                                        R"(
  semantics {
    sender: "BundledExchanges primary_url Loader"
    description:
      "Navigation request for the primary_url provided by a BundledExchanges "
      "that is requested by the user. This does not trigger any network "
      "transaction directly, but access to an entry in a local file, or in a "
      "previously fetched resource over network."
    trigger: "The user navigates to a BundledExchanges."
    data: "Nothing."
    destination: LOCAL
  }
  policy {
    cookies_allowed: NO
    setting: "These requests cannot be disabled in settings."
    policy_exception_justification:
      "Not implemented. This request does not make any network transaction."
  }
  comments:
    "Usually the request accesses an entry in a local file that contains "
    "multiple archived entries. But once the feature is exposed to the public "
    "web API, the archive file can be streamed over network. In such case, the "
    "streaming should be provided by another URLLoader request that is issued "
    "by Blink, but based on a user initiated navigation."
  )");

// A class to provide a network::mojom::URLLoader interface to redirect a
// request to the BundledExchanges to the main resource url.
class PrimaryURLRedirectLoader final : public network::mojom::URLLoader {
 public:
  PrimaryURLRedirectLoader(
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)
      : client_(std::move(client)) {}

  void OnReadyToRedirect(const network::ResourceRequest& resource_request,
                         const GURL& url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(client_.is_connected());
    network::ResourceResponseHead response_head;
    response_head.encoded_data_length = 0;
    response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(
            base::StringPrintf("HTTP/1.1 %d %s\r\n", 303, "See Other")));

    net::RedirectInfo redirect_info = net::RedirectInfo::ComputeRedirectInfo(
        "GET", resource_request.url, resource_request.site_for_cookies,
        resource_request.update_first_party_url_on_redirect
            ? net::URLRequest::FirstPartyURLPolicy::
                  UPDATE_FIRST_PARTY_URL_ON_REDIRECT
            : net::URLRequest::FirstPartyURLPolicy::
                  NEVER_CHANGE_FIRST_PARTY_URL,
        resource_request.referrer_policy, resource_request.referrer.spec(), 303,
        url, /*referrer_policy_header=*/base::nullopt,
        /*insecure_scheme_was_upgraded=*/false, /*copy_fragment=*/true,
        /*is_signed_exchange_fallback_redirect=*/false);
    client_->OnReceiveRedirect(redirect_info, response_head);
  }

 private:
  // mojom::URLLoader overrides:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<network::mojom::URLLoaderClient> client_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryURLRedirectLoader);
};

// A class to inherit NavigationLoaderInterceptor for the navigation to a
// BundledExchanges.
class Interceptor final : public NavigationLoaderInterceptor {
 public:
  using RepeatingRequestHandler = base::RepeatingCallback<void(
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderRequest,
      network::mojom::URLLoaderClientPtr)>;

  explicit Interceptor(RepeatingRequestHandler request_handler)
      : request_handler_(request_handler) {}
  ~Interceptor() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::move(callback).Run(request_handler_);
  }

  RepeatingRequestHandler request_handler_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

}  // namespace

BundledExchangesHandle::BundledExchangesHandle() {}

BundledExchangesHandle::BundledExchangesHandle(
    std::unique_ptr<BundledExchangesSource> bundled_exchanges_source)
    : source_(std::move(bundled_exchanges_source)),
      reader_(std::make_unique<BundledExchangesReader>(*source_)) {
  DCHECK(source_->is_trusted());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  reader_->ReadMetadata(base::BindOnce(&BundledExchangesHandle::OnMetadataReady,
                                       weak_factory_.GetWeakPtr()));
}

BundledExchangesHandle::~BundledExchangesHandle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

std::unique_ptr<NavigationLoaderInterceptor>
BundledExchangesHandle::CreateInterceptor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::make_unique<Interceptor>(
      reader_ ? base::BindRepeating(&BundledExchangesHandle::CreateURLLoader,
                                    weak_factory_.GetWeakPtr())
              : base::NullCallback());
}

void BundledExchangesHandle::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(url_loader_factory_);

  url_loader_factory_->SetFallbackFactory(std::move(fallback_factory));
  url_loader_factory_->Clone(std::move(receiver));
}

bool BundledExchangesHandle::IsReadyForLoading() {
  return !!url_loader_factory_;
}

void BundledExchangesHandle::CreateURLLoader(
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (metadata_error_) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_BUNDLED_EXCHANGES));
    return;
  }

  if (!url_loader_factory_) {
    // This must be the first request to the bundled exchange file.
    DCHECK_EQ(source_->url(), resource_request.url);
    pending_create_url_loader_task_ = base::Bind(
        &BundledExchangesHandle::CreateURLLoader, base::Unretained(this),
        resource_request, base::Passed(&request), base::Passed(&client));
    return;
  }

  // Currently |source_| must be a local file. And the bundle's primary URL
  // can't be a local file URL. So while handling redirected request to the
  // primary URL, |resource_request.url| must not be same as the |source|'s URL.
  if (source_->url() != resource_request.url) {
    url_loader_factory_->CreateLoaderAndStart(
        std::move(request), /*routing_id=*/0, /*request_id=*/0, /*options=*/0,
        resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
    return;
  }

  auto redirect_loader =
      std::make_unique<PrimaryURLRedirectLoader>(client.PassInterface());
  redirect_loader->OnReadyToRedirect(resource_request, primary_url_);
  mojo::MakeSelfOwnedReceiver(
      std::move(redirect_loader),
      mojo::PendingReceiver<network::mojom::URLLoader>(std::move(request)));
}

void BundledExchangesHandle::OnMetadataReady(
    data_decoder::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!url_loader_factory_);

  if (error) {
    metadata_error_ = std::move(error);
  } else {
    primary_url_ = reader_->GetPrimaryURL();
    url_loader_factory_ =
        std::make_unique<BundledExchangesURLLoaderFactory>(std::move(reader_));
  }

  if (pending_create_url_loader_task_)
    std::move(pending_create_url_loader_task_).Run();
}

}  // namespace content
