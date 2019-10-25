// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/blob/blob_url_null_origin_map.h"

#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
ThreadSpecific<BlobURLNullOriginMap>& BlobURLNullOriginMap::GetInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<BlobURLNullOriginMap>, map,
                                  ());
  return map;
}

void BlobURLNullOriginMap::Add(const KURL& blob_url, SecurityOrigin* origin) {
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
  DCHECK(!blob_url.HasFragmentIdentifier());
  DCHECK(origin->SerializesAsNull());
  blob_url_null_origin_map_.insert(blob_url.GetString(), origin);
}

void BlobURLNullOriginMap::Remove(const KURL& blob_url) {
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
  blob_url_null_origin_map_.erase(blob_url.GetString());
}

SecurityOrigin* BlobURLNullOriginMap::Get(const KURL& blob_url) {
  DCHECK(blob_url.ProtocolIs("blob"));
  DCHECK_EQ(BlobURL::GetOrigin(blob_url), "null");
  KURL blob_url_without_fragment = blob_url;
  blob_url_without_fragment.RemoveFragmentIdentifier();
  return blob_url_null_origin_map_.at(blob_url_without_fragment.GetString());
}

}  // namespace blink
