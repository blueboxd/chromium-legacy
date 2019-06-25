// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/prefetched_signed_exchange_manager.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_load_timing.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/alternate_signed_exchange_resource_info.h"
#include "third_party/blink/renderer/platform/loader/link_header.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class PrefetchedSignedExchangeManager::PrefetchedSignedExchangeLoader
    : public WebURLLoader {
 public:
  PrefetchedSignedExchangeLoader(
      const WebURLRequest& request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : request_(request),
        task_runner_(std::move(task_runner)),
        weak_ptr_factory_(this) {}

  ~PrefetchedSignedExchangeLoader() override {}

  base::WeakPtr<PrefetchedSignedExchangeLoader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetURLLoader(std::unique_ptr<WebURLLoader> url_loader) {
    DCHECK(!url_loader_);
    url_loader_ = std::move(url_loader);
    ExecutePendingMethodCalls();
  }

  const WebURLRequest& request() const { return request_; }

  // WebURLLoader methods:
  void LoadSynchronously(const WebURLRequest& request,
                         WebURLLoaderClient* client,
                         WebURLResponse& response,
                         base::Optional<WebURLError>& error,
                         WebData& data,
                         int64_t& encoded_data_length,
                         int64_t& encoded_body_length,
                         WebBlobInfo& downloaded_blob) override {
    NOTREACHED();
  }
  void LoadAsynchronously(const WebURLRequest& request,
                          WebURLLoaderClient* client) override {
    if (url_loader_) {
      url_loader_->LoadAsynchronously(request, client);
      return;
    }
    // It is safe to use Unretained(client), because |client| is a
    // ResourceLoader which owns |this|, and we are binding with weak ptr of
    // |this| here.
    pending_method_calls_.push(
        WTF::Bind(&PrefetchedSignedExchangeLoader::LoadAsynchronously,
                  GetWeakPtr(), request, WTF::Unretained(client)));
  }
  void SetDefersLoading(bool value) override {
    if (url_loader_) {
      url_loader_->SetDefersLoading(value);
      return;
    }
    pending_method_calls_.push(
        WTF::Bind(&PrefetchedSignedExchangeLoader::SetDefersLoading,
                  GetWeakPtr(), value));
  }
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value) override {
    if (url_loader_) {
      url_loader_->DidChangePriority(new_priority, intra_priority_value);
      return;
    }
    pending_method_calls_.push(
        WTF::Bind(&PrefetchedSignedExchangeLoader::DidChangePriority,
                  GetWeakPtr(), new_priority, intra_priority_value));
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return task_runner_;
  }

 private:
  void ExecutePendingMethodCalls() {
    std::queue<base::OnceClosure> pending_calls =
        std::move(pending_method_calls_);
    while (!pending_calls.empty()) {
      std::move(pending_calls.front()).Run();
      pending_calls.pop();
    }
  }

  const WebURLRequest request_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<WebURLLoader> url_loader_;
  std::queue<base::OnceClosure> pending_method_calls_;

  base::WeakPtrFactory<PrefetchedSignedExchangeLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchedSignedExchangeLoader);
};

// static
PrefetchedSignedExchangeManager* PrefetchedSignedExchangeManager::MaybeCreate(
    LocalFrame* frame,
    const String& outer_link_header,
    const String& inner_link_header,
    WebVector<std::unique_ptr<WebNavigationParams::PrefetchedSignedExchange>>
        prefetched_signed_exchanges) {
  DCHECK(RuntimeEnabledFeatures::SignedExchangeSubresourcePrefetchEnabled());
  if (prefetched_signed_exchanges.empty())
    return nullptr;
  std::unique_ptr<AlternateSignedExchangeResourceInfo> alternative_resources =
      AlternateSignedExchangeResourceInfo::CreateIfValid(outer_link_header,
                                                         inner_link_header);
  if (!alternative_resources) {
    // There is no "allowed-alt-sxg" link header for this resource.
    return nullptr;
  }

  HashMap<KURL, std::unique_ptr<WebNavigationParams::PrefetchedSignedExchange>>
      prefetched_exchanges_map;
  for (auto& exchange : prefetched_signed_exchanges) {
    const KURL outer_url = exchange->outer_url;
    prefetched_exchanges_map.Set(outer_url, std::move(exchange));
  }

  return MakeGarbageCollected<PrefetchedSignedExchangeManager>(
      frame, std::move(alternative_resources),
      std::move(prefetched_exchanges_map));
}

PrefetchedSignedExchangeManager::PrefetchedSignedExchangeManager(
    LocalFrame* frame,
    std::unique_ptr<AlternateSignedExchangeResourceInfo> alternative_resources,
    HashMap<KURL,
            std::unique_ptr<WebNavigationParams::PrefetchedSignedExchange>>
        prefetched_exchanges_map)
    : frame_(frame),
      alternative_resources_(std::move(alternative_resources)),
      prefetched_exchanges_map_(std::move(prefetched_exchanges_map)) {
  DCHECK(RuntimeEnabledFeatures::SignedExchangeSubresourcePrefetchEnabled());
}

PrefetchedSignedExchangeManager::~PrefetchedSignedExchangeManager() {}

void PrefetchedSignedExchangeManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
}

void PrefetchedSignedExchangeManager::StartPrefetchedLinkHeaderPreloads() {
  DCHECK(!started_);
  started_ = true;
  TriggerLoad();
  // Clears |prefetched_exchanges_map_| to release URLLoaderFactory in the
  // browser process.
  prefetched_exchanges_map_.clear();
  // Clears |alternative_resources_| which will not be used forever.
  alternative_resources_.reset();
}

std::unique_ptr<WebURLLoader>
PrefetchedSignedExchangeManager::MaybeCreateURLLoader(
    const WebURLRequest& request) {
  if (started_)
    return nullptr;
  const auto* matching_resource =
      alternative_resources_->FindMatchingEntry(request.Url());
  if (!matching_resource)
    return nullptr;

  std::unique_ptr<PrefetchedSignedExchangeLoader> loader =
      std::make_unique<PrefetchedSignedExchangeLoader>(
          request, frame_->GetFrameScheduler()->GetTaskRunner(
                       TaskType::kInternalLoading));
  loaders_.emplace_back(loader->GetWeakPtr());
  return loader;
}

std::unique_ptr<WebURLLoader>
PrefetchedSignedExchangeManager::CreateDefaultURLLoader(
    const WebURLRequest& request) {
  return frame_->GetURLLoaderFactory()->CreateURLLoader(
      request,
      frame_->GetFrameScheduler()->CreateResourceLoadingTaskRunnerHandle());
}

std::unique_ptr<WebURLLoader>
PrefetchedSignedExchangeManager::CreatePrefetchedSignedExchangeURLLoader(
    const WebURLRequest& request,
    network::mojom::blink::URLLoaderFactoryPtr loader_factory) {
  return Platform::Current()
      ->WrapURLLoaderFactory(loader_factory.PassInterface().PassHandle())
      ->CreateURLLoader(
          request,
          frame_->GetFrameScheduler()->CreateResourceLoadingTaskRunnerHandle());
}

void PrefetchedSignedExchangeManager::TriggerLoad() {
  std::vector<WebNavigationParams::PrefetchedSignedExchange*>
      maching_prefetched_exchanges;
  for (auto loader : loaders_) {
    if (!loader) {
      // The loader has been canceled.
      maching_prefetched_exchanges.emplace_back(nullptr);
      // We can continue the matching, because the distributor can't send
      // arbitrary information to the publisher using this resource.
      continue;
    }
    const auto* matching_resource =
        alternative_resources_->FindMatchingEntry(loader->request().Url());
    const auto alternative_url = matching_resource->alternative_url();
    if (!alternative_url.IsValid()) {
      // There is no matching "alternate" link header in outer response header.
      break;
    }
    const auto exchange_it = prefetched_exchanges_map_.find(alternative_url);
    if (exchange_it == prefetched_exchanges_map_.end()) {
      // There is no matching prefetched exchange.
      break;
    }
    if (String(exchange_it->value->header_integrity) !=
        matching_resource->header_integrity()) {
      // The header integrity doesn't match.
      break;
    }
    if (KURL(exchange_it->value->inner_url) !=
        matching_resource->anchor_url()) {
      // The inner URL doesn't match.
      break;
    }
    maching_prefetched_exchanges.emplace_back(exchange_it->value.get());
  }
  if (loaders_.size() != maching_prefetched_exchanges.size()) {
    // Need to load the all original resources in this case to prevent the
    // distributor from sending arbitrary information to the publisher.
    String message =
        "Failed to match prefetched alternative signed exchange subresources. "
        "Requesting the all original resources ignoreing all alternative signed"
        " exchange responses.";
    frame_->GetDocument()->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kNetwork,
                               mojom::ConsoleMessageLevel::kError, message));
    for (auto loader : loaders_) {
      if (!loader)
        continue;
      loader->SetURLLoader(CreateDefaultURLLoader(loader->request()));
    }
    return;
  }
  for (wtf_size_t i = 0; i < loaders_.size(); ++i) {
    auto loader = loaders_.at(i);
    if (!loader)
      continue;
    auto* prefetched_exchange = maching_prefetched_exchanges.at(i);
    network::mojom::blink::URLLoaderFactoryPtr loader_factory =
        network::mojom::blink::URLLoaderFactoryPtr(
            network::mojom::blink::URLLoaderFactoryPtrInfo(
                std::move(prefetched_exchange->loader_factory_handle),
                network::mojom::URLLoaderFactory::Version_));
    network::mojom::blink::URLLoaderFactoryPtr loader_factory_clone;
    loader_factory->Clone(MakeRequest(&loader_factory_clone));
    // Reset loader_factory_handle to support loading the same resource again.
    prefetched_exchange->loader_factory_handle =
        loader_factory_clone.PassInterface().PassHandle();
    loader->SetURLLoader(CreatePrefetchedSignedExchangeURLLoader(
        loader->request(), std::move(loader_factory)));
  }
}

}  // namespace blink
