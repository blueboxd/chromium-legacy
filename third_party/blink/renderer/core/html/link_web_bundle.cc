// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_web_bundle.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/web_bundle_subresource_loader.h"

namespace blink {

// WebBundleLoader is responsible for loading a WebBundle resource.
class WebBundleLoader : public GarbageCollected<WebBundleLoader>,
                        public ThreadableLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(WebBundleLoader);

 public:
  WebBundleLoader(LinkWebBundle& link_web_bundle,
                  ExecutionContext& execution_context,
                  const KURL& url)
      : link_web_bundle_(&link_web_bundle),
        pending_factory_receiver_(
            loader_factory_.BindNewPipeAndPassReceiver()) {
    ResourceRequest request(url);
    request.SetUseStreamOnResponse(true);
    // TODO(crbug.com/1082020): Revisit these once the fetch and process the
    // linked resource algorithm [1] for <link rel=webbundle> is defined.
    // [1]
    // https://html.spec.whatwg.org/multipage/semantics.html#fetch-and-process-the-linked-resource
    request.SetRequestContext(mojom::blink::RequestContextType::SUBRESOURCE);
    request.SetMode(network::mojom::blink::RequestMode::kCors);
    request.SetCredentialsMode(network::mojom::blink::CredentialsMode::kOmit);

    ResourceLoaderOptions resource_loader_options;
    resource_loader_options.data_buffering_policy = kDoNotBufferData;

    loader_ = MakeGarbageCollected<ThreadableLoader>(execution_context, this,
                                                     resource_loader_options);
    loader_->Start(std::move(request));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(link_web_bundle_);
    visitor->Trace(loader_);
  }

  bool HasLoaded() const { return !failed_; }

  mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
  GetURLLoaderFactory() {
    mojo::PendingRemote<network::mojom::blink::URLLoaderFactory> factory_clone;
    loader_factory_->Clone(factory_clone.InitWithNewPipeAndPassReceiver());
    return factory_clone;
  }

  // ThreadableLoaderClient
  void DidReceiveResponse(uint64_t, const ResourceResponse& response) override {
    if (!cors::IsOkStatus(response.HttpStatusCode()))
      failed_ = true;
    // TODO(crbug.com/1082020): Check response headers, as spec'ed in
    // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#name-serving-constraints.
  }

  void DidStartLoadingResponseBody(BytesConsumer& consumer) override {
    DCHECK(pending_factory_receiver_);
    CreateWebBundleSubresourceLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory>(
            pending_factory_receiver_.PassPipe()),
        consumer.DrainAsDataPipe());
    // TODO(crbug.com/1082020): Set |failed_| to true on metadata parse error,
    // so that "error" event is dispatched.
  }

  void DidFinishLoading(uint64_t) override { link_web_bundle_->NotifyLoaded(); }
  void DidFail(const ResourceError&) override { DidFailInternal(); }
  void DidFailRedirectCheck() override { DidFailInternal(); }

 private:
  void DidFailInternal() {
    if (pending_factory_receiver_) {
      // If we haven't create a WebBundleSubresourceLoaderFactory, create it
      // with an empty bundle body so that requests to
      // |pending_factory_receiver_| are processed (and fail).
      CreateWebBundleSubresourceLoaderFactory(
          mojo::PendingReceiver<network::mojom::URLLoaderFactory>(
              pending_factory_receiver_.PassPipe()),
          mojo::ScopedDataPipeConsumerHandle());
    }
    failed_ = true;
    link_web_bundle_->NotifyLoaded();
  }

  Member<LinkWebBundle> link_web_bundle_;
  Member<ThreadableLoader> loader_;
  mojo::Remote<network::mojom::blink::URLLoaderFactory> loader_factory_;
  mojo::PendingReceiver<network::mojom::blink::URLLoaderFactory>
      pending_factory_receiver_;
  bool failed_ = false;
};

LinkWebBundle::LinkWebBundle(HTMLLinkElement* owner) : LinkResource(owner) {}
LinkWebBundle::~LinkWebBundle() = default;

void LinkWebBundle::Trace(Visitor* visitor) const {
  visitor->Trace(bundle_loader_);
  LinkResource::Trace(visitor);
  SubresourceWebBundle::Trace(visitor);
}

void LinkWebBundle::NotifyLoaded() {
  if (owner_)
    owner_->ScheduleEvent();
}

void LinkWebBundle::Process() {
  if (!owner_ || !owner_->GetDocument().GetFrame())
    return;
  if (!owner_->ShouldLoadLink())
    return;

  ResourceFetcher* resource_fetcher = owner_->GetDocument().Fetcher();
  if (!resource_fetcher)
    return;

  bundle_loader_ = MakeGarbageCollected<WebBundleLoader>(
      *this, *owner_->GetDocument().GetExecutionContext(), owner_->Href());

  resource_fetcher->AddSubresourceWebBundle(*this);
}

LinkResource::LinkResourceType LinkWebBundle::GetType() const {
  return kOther;
}

bool LinkWebBundle::HasLoaded() const {
  return bundle_loader_ && bundle_loader_->HasLoaded();
}

void LinkWebBundle::OwnerRemoved() {
  if (!owner_)
    return;
  ResourceFetcher* resource_fetcher = owner_->GetDocument().Fetcher();
  if (!resource_fetcher)
    return;
  resource_fetcher->RemoveSubresourceWebBundle(*this);
  bundle_loader_ = nullptr;
}

bool LinkWebBundle::CanHandleRequest(const KURL& url) const {
  return owner_ && owner_->ValidResourceUrls().Contains(url);
}

mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
LinkWebBundle::GetURLLoaderFactory() {
  DCHECK(bundle_loader_);
  return bundle_loader_->GetURLLoaderFactory();
}

// static
KURL LinkWebBundle::ParseResourceUrl(const AtomicString& str) {
  // The implementation is almost copy and paste from ParseExchangeURL() defined
  // in services/data_decoder/web_bundle_parser.cc, replacing GURL with KURL.

  // TODO(hayato): Consider to support a relative URL.
  KURL url(str);
  if (!url.IsValid())
    return KURL();

  // Exchange URL must not have a fragment or credentials.
  if (url.HasFragmentIdentifier() || !url.User().IsEmpty() ||
      !url.Pass().IsEmpty())
    return KURL();

  // For now, we allow only http: and https: schemes in Web Bundle URLs.
  // TODO(crbug.com/966753): Revisit this once
  // https://github.com/WICG/webpackage/issues/468 is resolved.
  if (!url.ProtocolIsInHTTPFamily())
    return KURL();

  return url;
}

}  // namespace blink
