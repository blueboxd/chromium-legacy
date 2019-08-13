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

// A class to provide a network::mojom::URLLoader interface to redirect a
// request to the BundledExchanges to the main resource url.
class BundledExchangesHandle::PrimaryURLRedirectLoader final
    : public network::mojom::URLLoader {
 public:
  PrimaryURLRedirectLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)
      : client_(std::move(client)) {
    redirect_info_ = net::RedirectInfo::ComputeRedirectInfo(
        "GET", resource_request.url, resource_request.site_for_cookies,
        resource_request.top_frame_origin,
        resource_request.update_first_party_url_on_redirect
            ? net::URLRequest::FirstPartyURLPolicy::
                  UPDATE_FIRST_PARTY_URL_ON_REDIRECT
            : net::URLRequest::FirstPartyURLPolicy::
                  NEVER_CHANGE_FIRST_PARTY_URL,
        resource_request.referrer_policy, resource_request.referrer.spec(), 303,
        GURL(), /*referrer_policy_header=*/base::nullopt,
        /*insecure_scheme_was_upgraded=*/false, /*copy_fragment=*/true,
        /*is_signed_exchange_fallback_redirect=*/false);
  }

  void OnReadyToRedirect(
      const GURL& url,
      data_decoder::mojom::BundleMetadataParseErrorPtr error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(client_.is_connected());

    // TODO(crbug.com/966753): Handle |error|.

    network::ResourceResponseHead response_head;
    response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(
            base::StringPrintf("HTTP/1.1 %d %s\r\n", 303, "See Other")));
    redirect_info_.new_url = url;
    client_->OnReceiveRedirect(redirect_info_, response_head);
  }

  base::WeakPtr<PrimaryURLRedirectLoader> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
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
  net::RedirectInfo redirect_info_;

  base::WeakPtrFactory<PrimaryURLRedirectLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrimaryURLRedirectLoader);
};

BundledExchangesHandle::BundledExchangesHandle(
    const BundledExchangesSource& bundled_exchanges_source)
    : source_(bundled_exchanges_source),
      reader_(
          std::make_unique<BundledExchangesReader>(bundled_exchanges_source)) {
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
      source_.IsValid()
          ? base::BindRepeating(&BundledExchangesHandle::CreatePrimaryURLLoader,
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

void BundledExchangesHandle::CreatePrimaryURLLoader(
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check if this is the initial request to the BundledExchanges itself, or
  // subsequent requests to the primary exchange in the BundledExchanges.
  if (!is_redirected_) {
    DCHECK(source_.Match(resource_request.url));
    DCHECK(!redirect_loader_);

    // This is the initial request to the BundledExchanges.
    is_redirected_ = true;
    auto redirect_loader =
        std::make_unique<BundledExchangesHandle::PrimaryURLRedirectLoader>(
            resource_request, client.PassInterface());
    redirect_loader_ = redirect_loader->GetWeakPtr();
    std::unique_ptr<network::mojom::URLLoader> url_loader(
        std::move(redirect_loader));
    mojo::MakeSelfOwnedReceiver(
        std::move(url_loader),
        mojo::PendingReceiver<network::mojom::URLLoader>(std::move(request)));
    MayRedirectPrimaryURLLoader();
  } else {
    DCHECK(url_loader_factory_);

    url_loader_factory_->CreateLoaderAndStart(
        std::move(request), /*routing_id=*/0, /*request_id=*/0, /*options=*/0,
        resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
  }
}

void BundledExchangesHandle::MayRedirectPrimaryURLLoader() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // CreatedPrimaryURLLoader() and OnMetadataReady() both should be called
  // beforehand. Otherwise, do nothing and wait the next call.
  if (!redirect_loader_ || (!primary_url_.is_valid() && !metadata_error_))
    return;

  redirect_loader_->OnReadyToRedirect(std::move(primary_url_),
                                      std::move(metadata_error_));
  redirect_loader_.reset();
}

void BundledExchangesHandle::OnMetadataReady(
    data_decoder::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!url_loader_factory_);

  primary_url_ = std::move(reader_->GetPrimaryURL());
  metadata_error_ = std::move(error);

  url_loader_factory_ =
      std::make_unique<BundledExchangesURLLoaderFactory>(std::move(reader_));

  MayRedirectPrimaryURLLoader();
}

}  // namespace content
