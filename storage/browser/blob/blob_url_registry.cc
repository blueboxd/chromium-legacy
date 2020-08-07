// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_registry.h"

#include "base/check.h"
#include "storage/browser/blob/blob_url_utils.h"
#include "url/gurl.h"

namespace storage {

BlobUrlRegistry::BlobUrlRegistry() = default;
BlobUrlRegistry::~BlobUrlRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool BlobUrlRegistry::AddUrlMapping(
    const GURL& blob_url,
    mojo::PendingRemote<blink::mojom::Blob> blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!BlobUrlUtils::UrlHasFragment(blob_url));
  if (IsUrlMapped(blob_url))
    return false;
  url_to_blob_[blob_url] = mojo::Remote<blink::mojom::Blob>(std::move(blob));
  return true;
}

bool BlobUrlRegistry::RemoveUrlMapping(const GURL& blob_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!BlobUrlUtils::UrlHasFragment(blob_url));
  auto it = url_to_blob_.find(blob_url);
  if (it == url_to_blob_.end())
    return false;
  url_to_blob_.erase(it);
  return true;
}

bool BlobUrlRegistry::IsUrlMapped(const GURL& blob_url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_to_blob_.find(blob_url) != url_to_blob_.end();
}

mojo::PendingRemote<blink::mojom::Blob> BlobUrlRegistry::GetBlobFromUrl(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = url_to_blob_.find(BlobUrlUtils::ClearUrlFragment(url));
  if (it == url_to_blob_.end())
    return mojo::NullRemote();
  mojo::PendingRemote<blink::mojom::Blob> result;
  it->second->Clone(result.InitWithNewPipeAndPassReceiver());
  return result;
}

void BlobUrlRegistry::AddTokenMapping(
    const base::UnguessableToken& token,
    const GURL& url,
    mojo::PendingRemote<blink::mojom::Blob> blob) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(token_to_url_and_blob_.find(token) == token_to_url_and_blob_.end());
  token_to_url_and_blob_.emplace(
      token,
      std::make_pair(url, mojo::Remote<blink::mojom::Blob>(std::move(blob))));
}

void BlobUrlRegistry::RemoveTokenMapping(const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(token_to_url_and_blob_.find(token) != token_to_url_and_blob_.end());
  token_to_url_and_blob_.erase(token);
}

bool BlobUrlRegistry::GetTokenMapping(
    const base::UnguessableToken& token,
    GURL* url,
    mojo::PendingRemote<blink::mojom::Blob>* blob) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = token_to_url_and_blob_.find(token);
  if (it == token_to_url_and_blob_.end())
    return false;
  *url = it->second.first;
  it->second.second->Clone(blob->InitWithNewPipeAndPassReceiver());
  return true;
}

}  // namespace storage
