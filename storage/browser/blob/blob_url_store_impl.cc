// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_store_impl.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_loader_factory.h"
#include "storage/browser/blob/blob_url_utils.h"

namespace storage {

// Self deletes when the last binding to it is closed.
class BlobURLTokenImpl : public blink::mojom::BlobURLToken {
 public:
  BlobURLTokenImpl(base::WeakPtr<BlobStorageContext> context,
                   const GURL& url,
                   std::unique_ptr<BlobDataHandle> blob,
                   mojo::PendingReceiver<blink::mojom::BlobURLToken> receiver)
      : context_(std::move(context)),
        url_(url),
        blob_(std::move(blob)),
        token_(base::UnguessableToken::Create()) {
    receivers_.Add(this, std::move(receiver));
    receivers_.set_disconnect_handler(base::BindRepeating(
        &BlobURLTokenImpl::OnConnectionError, base::Unretained(this)));
    if (context_) {
      context_->mutable_registry()->AddTokenMapping(token_, url_,
                                                    blob_->uuid());
    }
  }

  ~BlobURLTokenImpl() override {
    if (context_)
      context_->mutable_registry()->RemoveTokenMapping(token_);
  }

  void GetToken(GetTokenCallback callback) override {
    std::move(callback).Run(token_);
  }

  void Clone(
      mojo::PendingReceiver<blink::mojom::BlobURLToken> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  void OnConnectionError() {
    if (!receivers_.empty())
      return;
    delete this;
  }

  base::WeakPtr<BlobStorageContext> context_;
  mojo::ReceiverSet<blink::mojom::BlobURLToken> receivers_;
  const GURL url_;
  const std::unique_ptr<BlobDataHandle> blob_;
  const base::UnguessableToken token_;
};

BlobURLStoreImpl::BlobURLStoreImpl(base::WeakPtr<BlobStorageContext> context,
                                   BlobRegistryImpl::Delegate* delegate)
    : context_(std::move(context)), delegate_(delegate) {}

BlobURLStoreImpl::~BlobURLStoreImpl() {
  if (context_) {
    for (const auto& url : urls_)
      context_->RevokePublicBlobURL(url);
  }
}

void BlobURLStoreImpl::Register(mojo::PendingRemote<blink::mojom::Blob> blob,
                                const GURL& url,
                                RegisterCallback callback) {
  if (!url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Invalid scheme passed to BlobURLStore::Register");
    std::move(callback).Run();
    return;
  }
  // Only report errors when we don't have permission to commit and
  // the process is valid. The process check is a temporary solution to
  // handle cases where this method is run after the
  // process associated with |delegate_| has been destroyed.
  // See https://crbug.com/933089 for details.
  if (!delegate_->CanCommitURL(url) && delegate_->IsProcessValid()) {
    mojo::ReportBadMessage(
        "Non committable URL passed to BlobURLStore::Register");
    std::move(callback).Run();
    return;
  }
  if (BlobUrlUtils::UrlHasFragment(url)) {
    mojo::ReportBadMessage(
        "URL with fragment passed to BlobURLStore::Register");
    std::move(callback).Run();
    return;
  }

  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob));
  blink::mojom::Blob* blob_ptr = blob_remote.get();
  blob_ptr->GetInternalUUID(base::BindOnce(
      &BlobURLStoreImpl::RegisterWithUUID, weak_ptr_factory_.GetWeakPtr(),
      std::move(blob_remote), url, std::move(callback)));
}

void BlobURLStoreImpl::Revoke(const GURL& url) {
  if (!url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Invalid scheme passed to BlobURLStore::Revoke");
    return;
  }
  // Only report errors when we don't have permission to commit and
  // the process is valid. The process check is a temporary solution to
  // handle cases where this method is run after the
  // process associated with |delegate_| has been destroyed.
  // See https://crbug.com/933089 for details.
  if (!delegate_->CanCommitURL(url) && delegate_->IsProcessValid()) {
    mojo::ReportBadMessage(
        "Non committable URL passed to BlobURLStore::Revoke");
    return;
  }
  if (BlobUrlUtils::UrlHasFragment(url)) {
    mojo::ReportBadMessage("URL with fragment passed to BlobURLStore::Revoke");
    return;
  }

  if (context_)
    context_->RevokePublicBlobURL(url);
  urls_.erase(url);
}

void BlobURLStoreImpl::Resolve(const GURL& url, ResolveCallback callback) {
  if (!context_) {
    std::move(callback).Run(nullptr);
    return;
  }
  blink::mojom::BlobPtr blob;
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->GetBlobDataFromPublicURL(url);
  if (blob_handle)
    BlobImpl::Create(std::move(blob_handle), MakeRequest(&blob));
  std::move(callback).Run(std::move(blob));
}

void BlobURLStoreImpl::ResolveAsURLLoaderFactory(
    const GURL& url,
    network::mojom::URLLoaderFactoryRequest request) {
  BlobURLLoaderFactory::Create(
      context_ ? context_->GetBlobDataFromPublicURL(url) : nullptr, url,
      std::move(request));
}

void BlobURLStoreImpl::ResolveForNavigation(
    const GURL& url,
    mojo::PendingReceiver<blink::mojom::BlobURLToken> token) {
  if (!context_)
    return;
  std::unique_ptr<BlobDataHandle> blob_handle =
      context_->GetBlobDataFromPublicURL(url);
  if (!blob_handle)
    return;
  new BlobURLTokenImpl(context_, url, std::move(blob_handle), std::move(token));
}

void BlobURLStoreImpl::RegisterWithUUID(mojo::Remote<blink::mojom::Blob> blob,
                                        const GURL& url,
                                        RegisterCallback callback,
                                        const std::string& uuid) {
  // |blob| is unused, but is passed here to be kept alive until
  // RegisterPublicBlobURL increments the refcount of it via the uuid.
  if (context_)
    context_->RegisterPublicBlobURL(url, uuid);
  urls_.insert(url);
  std::move(callback).Run();
}

}  // namespace storage
