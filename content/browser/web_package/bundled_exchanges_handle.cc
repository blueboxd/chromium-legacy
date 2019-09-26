// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_handle.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "content/browser/web_package/bundled_exchanges_url_loader_factory.h"
#include "content/browser/web_package/bundled_exchanges_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"
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
// untrustable BundledExchanges file (eg: "file:///tmp/a.wbn").
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for all navigation requests. It calls the
//     |callback| with the a null RequestHandler.
// [2] MaybeCreateLoaderForResponse() is called for all navigation responses.
//     If the response mime type is not "application/webbundle", returns false.
//     Otherwise starts reading the metadata and returns true. Once the metadata
//     is read, OnMetadataReady() is called, and a redirect loader is
//     created to redirect the navigation request to the Bundle's synthesized
//     primary URL ("file:///tmp/a.wbn?https://example.com/a.html").
// [3] MaybeCreateLoader() is called again for the redirect. It continues on
//     StartResponse() to create the loader for the main resource.
class InterceptorForFile final : public NavigationLoaderInterceptor {
 public:
  using DoneCallback = base::OnceCallback<void(
      const GURL& base_url_override,
      std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory)>;

  explicit InterceptorForFile(DoneCallback done_callback)
      : done_callback_(std::move(done_callback)) {}
  ~InterceptorForFile() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // InterceptorForFile::MaybeCreateLoader() creates a loader only after
    // recognising that the response is a bundled exchange file at
    // MaybeCreateLoaderForResponse() and successfully created
    // |url_loader_factory_|.
    if (!url_loader_factory_) {
      std::move(callback).Run({});
      return;
    }
    std::move(callback).Run(base::BindOnce(&InterceptorForFile::StartResponse,
                                           weak_factory_.GetWeakPtr()));
  }

  bool MaybeCreateLoaderForResponse(
      const network::ResourceRequest& request,
      const network::ResourceResponseHead& response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      network::mojom::URLLoaderPtr* loader,
      network::mojom::URLLoaderClientRequest* client_request,
      ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors,
      bool* will_return_unsafe_redirect) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(bundled_exchanges_utils::IsSupprtedFileScheme(request.url));
    if (response_head.mime_type !=
        bundled_exchanges_utils::
            kBundledExchangesFileMimeTypeWithoutParameters) {
      return false;
    }
    std::unique_ptr<BundledExchangesSource> source =
        BundledExchangesSource::MaybeCreateFromFileUrl(request.url);
    if (!source)
      return false;
    reader_ = std::make_unique<BundledExchangesReader>(*source);
    reader_->ReadMetadata(base::BindOnce(&InterceptorForFile::OnMetadataReady,
                                         weak_factory_.GetWeakPtr(), request));
    *client_request = forwarding_client_.BindNewPipeAndPassReceiver();
    *will_return_unsafe_redirect = true;
    return true;
  }

  void OnMetadataReady(
      network::ResourceRequest request,
      data_decoder::mojom::BundleMetadataParseErrorPtr metadata_error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (metadata_error) {
      forwarding_client_->OnComplete(network::URLLoaderCompletionStatus(
          net::ERR_INVALID_BUNDLED_EXCHANGES));
      forwarding_client_.reset();
      return;
    }
    DCHECK(reader_);
    primary_url_ = reader_->GetPrimaryURL();
    url_loader_factory_ =
        std::make_unique<BundledExchangesURLLoaderFactory>(std::move(reader_));

    const GURL new_url =
        bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
            request.url, primary_url_);
    auto redirect_loader =
        std::make_unique<PrimaryURLRedirectLoader>(forwarding_client_.Unbind());
    redirect_loader->OnReadyToRedirect(request, new_url);
  }

  void StartResponse(const network::ResourceRequest& resource_request,
                     network::mojom::URLLoaderRequest request,
                     network::mojom::URLLoaderClientPtr client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    network::ResourceRequest new_resource_request = resource_request;
    new_resource_request.url = primary_url_;
    url_loader_factory_->CreateLoaderAndStart(
        std::move(request), /*routing_id=*/0, /*request_id=*/0, /*options=*/0,
        new_resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
    std::move(done_callback_).Run(primary_url_, std::move(url_loader_factory_));
  }

  DoneCallback done_callback_;
  std::unique_ptr<BundledExchangesReader> reader_;
  GURL primary_url_;
  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;

  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterceptorForFile> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterceptorForFile);
};

// A class to inherit NavigationLoaderInterceptor for the navigation to a
// trustable BundledExchanges file (eg: "file:///tmp/a.wbn").
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for the navigation request to the trustable
//     BundledExchanges file. It continues on CreateURLLoader() to create the
//     loader for the main resource.
//     - If OnMetadataReady() has not been called yet:
//         Wait for OnMetadataReady() to be called.
//     - If OnMetadataReady() was called with an error:
//         Completes the request with ERR_INVALID_BUNDLED_EXCHANGES.
//     - If OnMetadataReady() was called whthout errors:
//         A redirect loader is created to redirect the navigation request to
//         the Bundle's primary URL ("https://example.com/a.html").
// [2] MaybeCreateLoader() is called again for the redirect. It continues on
//     CreateURLLoader() to create the loader for the main resource.
class InterceptorForTrustableFile final : public NavigationLoaderInterceptor {
 public:
  using DoneCallback = base::OnceCallback<void(
      const GURL& base_url_override,
      std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory)>;

  InterceptorForTrustableFile(std::unique_ptr<BundledExchangesSource> source,
                              DoneCallback done_callback)
      : source_(std::move(source)),
        reader_(std::make_unique<BundledExchangesReader>(*source_)),
        done_callback_(std::move(done_callback)) {
    reader_->ReadMetadata(
        base::BindOnce(&InterceptorForTrustableFile::OnMetadataReady,
                       weak_factory_.GetWeakPtr()));
  }
  ~InterceptorForTrustableFile() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(
        base::BindOnce(&InterceptorForTrustableFile::CreateURLLoader,
                       weak_factory_.GetWeakPtr()));
  }

  void CreateURLLoader(const network::ResourceRequest& resource_request,
                       network::mojom::URLLoaderRequest request,
                       network::mojom::URLLoaderClientPtr client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (metadata_error_) {
      client->OnComplete(network::URLLoaderCompletionStatus(
          net::ERR_INVALID_BUNDLED_EXCHANGES));
      return;
    }

    if (!url_loader_factory_) {
      // This must be the first request to the bundled exchange file.
      DCHECK_EQ(source_->url(), resource_request.url);
      pending_resource_request_ = resource_request;
      pending_request_ = std::move(request);
      pending_client_ = std::move(client);
      return;
    }

    // Currently |source_| must be a local file. And the bundle's primary URL
    // can't be a local file URL. So while handling redirected request to the
    // primary URL, |resource_request.url| must not be same as the |source_|'s
    // URL.
    if (source_->url() != resource_request.url) {
      url_loader_factory_->CreateLoaderAndStart(
          std::move(request), /*routing_id=*/0, /*request_id=*/0, /*options=*/0,
          resource_request, std::move(client),
          net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
      std::move(done_callback_)
          .Run(
              /*base_url_override=*/GURL(), std::move(url_loader_factory_));
      return;
    }

    auto redirect_loader =
        std::make_unique<PrimaryURLRedirectLoader>(client.PassInterface());
    redirect_loader->OnReadyToRedirect(resource_request, primary_url_);
    mojo::MakeSelfOwnedReceiver(
        std::move(redirect_loader),
        mojo::PendingReceiver<network::mojom::URLLoader>(std::move(request)));
  }

  void OnMetadataReady(data_decoder::mojom::BundleMetadataParseErrorPtr error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!url_loader_factory_);

    if (error) {
      metadata_error_ = std::move(error);
    } else {
      primary_url_ = reader_->GetPrimaryURL();
      url_loader_factory_ = std::make_unique<BundledExchangesURLLoaderFactory>(
          std::move(reader_));
    }

    if (pending_request_) {
      DCHECK(pending_client_);
      CreateURLLoader(pending_resource_request_, std::move(pending_request_),
                      std::move(pending_client_));
    }
  }

  std::unique_ptr<BundledExchangesSource> source_;
  std::unique_ptr<BundledExchangesReader> reader_;
  DoneCallback done_callback_;

  network::ResourceRequest pending_resource_request_;
  network::mojom::URLLoaderRequest pending_request_;
  network::mojom::URLLoaderClientPtr pending_client_;

  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;

  GURL primary_url_;
  data_decoder::mojom::BundleMetadataParseErrorPtr metadata_error_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterceptorForTrustableFile> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterceptorForTrustableFile);
};

}  // namespace

// static
std::unique_ptr<BundledExchangesHandle>
BundledExchangesHandle::CreateForFile() {
  auto handle = base::WrapUnique(new BundledExchangesHandle());
  handle->SetInterceptor(std::make_unique<InterceptorForFile>(
      base::BindOnce(&BundledExchangesHandle::OnBundledExchangesFileLoaded,
                     handle->weak_factory_.GetWeakPtr())));
  return handle;
}

// static
std::unique_ptr<BundledExchangesHandle>
BundledExchangesHandle::CreateForTrustableFile(
    std::unique_ptr<BundledExchangesSource> source) {
  DCHECK(source->is_trusted());
  auto handle = base::WrapUnique(new BundledExchangesHandle());
  handle->SetInterceptor(std::make_unique<InterceptorForTrustableFile>(
      std::move(source),
      base::BindOnce(&BundledExchangesHandle::OnBundledExchangesFileLoaded,
                     handle->weak_factory_.GetWeakPtr())));
  return handle;
}

BundledExchangesHandle::BundledExchangesHandle() = default;

BundledExchangesHandle::~BundledExchangesHandle() = default;

std::unique_ptr<NavigationLoaderInterceptor>
BundledExchangesHandle::TakeInterceptor() {
  DCHECK(interceptor_);
  return std::move(interceptor_);
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

void BundledExchangesHandle::SetInterceptor(
    std::unique_ptr<NavigationLoaderInterceptor> interceptor) {
  interceptor_ = std::move(interceptor);
}

void BundledExchangesHandle::OnBundledExchangesFileLoaded(
    const GURL& base_url_override,
    std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory) {
  base_url_override_ = base_url_override;
  url_loader_factory_ = std::move(url_loader_factory);
}

}  // namespace content
