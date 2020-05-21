// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_LINK_FETCH_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_LINK_FETCH_RESOURCE_H_

#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

class FetchParameters;
class ResourceFetcher;

// This is the implementation of Resource for <link rel='prefetch'>.
// TODO(nhiroki): Rename this to LinkPrefetchResource.
class LinkFetchResource final : public Resource {
 public:
  static Resource* Fetch(FetchParameters&, ResourceFetcher*);

  LinkFetchResource(const ResourceRequest&,
                    const ResourceLoaderOptions&);
  ~LinkFetchResource() override;

 private:
  class LinkResourceFactory final : public NonTextResourceFactory {
   public:
    LinkResourceFactory();

    Resource* Create(const ResourceRequest& request,
                     const ResourceLoaderOptions& options) const override;
  };
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_LINK_FETCH_RESOURCE_H_
