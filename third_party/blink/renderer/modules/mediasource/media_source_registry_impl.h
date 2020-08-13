// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_registry.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

class KURL;

// This singleton lives on the main thread. It allows registration and
// deregistration of MediaSource objectUrls. Lookups to retrieve a reference to
// a registered MediaSource by its objectUrl are only allowed on the main
// thread; the only intended Lookup() caller is invoked by HTMLMediaElement's
// MSE attachment during element load.
// TODO(https://crbug.com/878133): Refactor this to allow registration and
// lookup of cross-thread (worker) MediaSource objectUrls.
class MediaSourceRegistryImpl final : public MediaSourceRegistry {
 public:
  // Creates the singleton instance. Must be run on the main thread (expected to
  // be done by modules initialization to ensure it happens early and on the
  // main thread.)
  static void Init();

  // MediaSourceRegistry : URLRegistry overrides for (un)registering blob URLs
  // referring to the specified media source attachment. RegisterURL creates a
  // scoped_refptr to manage the registrable's ref-counted lifetime and puts it
  // in |media_sources_|.
  void RegisterURL(SecurityOrigin*, const KURL&, URLRegistrable*) override;

  // UnregisterURL removes the corresponding scoped_refptr and KURL from
  // |media_sources_| if its KURL was there.
  void UnregisterURL(const KURL&) override;

  // MediaSourceRegistry override that finds |url| in |media_sources_| and
  // returns the corresponding scoped_refptr if found. Otherwise, returns an
  // unset scoped_refptr. |url| must be non-empty.
  scoped_refptr<MediaSourceAttachment> LookupMediaSource(
      const String& url) override;

 private:
  // Construction of this singleton informs MediaSourceAttachment of this
  // singleton, for it to use to service URLRegistry interface activities on
  // this registry like lookup, registration and unregistration.
  MediaSourceRegistryImpl();

  HashMap<String, scoped_refptr<MediaSourceAttachment>> media_sources_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASOURCE_MEDIA_SOURCE_REGISTRY_IMPL_H_
