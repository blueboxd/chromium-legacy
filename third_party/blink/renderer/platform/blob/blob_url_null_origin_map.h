// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_URL_NULL_ORIGIN_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_URL_NULL_ORIGIN_MAP_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

class KURL;
class SecurityOrigin;

// BlobURLNullOriginMap contains pairs of blob URL and security origin that is
// serialized into "null". An instance of this class is per-thread, and created
// when GetInstace() is called for the first time.
//
// When a blob URL is created in an opaque origin or something whose
// SecurityOrigin::SerializesAsNull() returns true, the origin is serialized
// into the URL as "null". Since that makes it impossible to parse the origin
// back out and compare it against a context's origin (to check if a context is
// allowed to dereference the URL), this class stores a map of blob URL to such
// an origin.
class PLATFORM_EXPORT BlobURLNullOriginMap {
 public:
  // Returns a thread-specific instance. The instance is created when this
  // function is called for the first time.
  static ThreadSpecific<BlobURLNullOriginMap>& GetInstance();

  // Adds a pair of |blob_url| and |origin| to the map. |blob_url| and |origin|
  // must have the same "null" origin.
  void Add(const KURL& blob_url, SecurityOrigin* origin);

  // Removes a "null" origin keyed with |blob_url| from the map. |blob_url| must
  // have the "null" origin.
  void Remove(const KURL& blob_url);

  // Returns a "null" origin keyed with |blob_url| from the map. |blob_url| must
  // have the "null" origin.
  SecurityOrigin* Get(const KURL& blob_url);

 private:
  friend class ThreadSpecific<BlobURLNullOriginMap>;

  HashMap<String, scoped_refptr<SecurityOrigin>> blob_url_null_origin_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_URL_NULL_ORIGIN_MAP_H_
