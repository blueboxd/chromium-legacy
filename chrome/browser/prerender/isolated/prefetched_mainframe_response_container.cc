// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/prefetched_mainframe_response_container.h"

PrefetchedMainframeResponseContainer::PrefetchedMainframeResponseContainer(
    const net::NetworkIsolationKey& nik,
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body)
    : network_isolation_key_(nik),
      head_(std::move(head)),
      body_(std::move(body)) {}

PrefetchedMainframeResponseContainer::~PrefetchedMainframeResponseContainer() =
    default;

network::mojom::URLResponseHeadPtr
PrefetchedMainframeResponseContainer::TakeHead() {
  DCHECK(head_);
  return std::move(head_);
}

std::unique_ptr<std::string> PrefetchedMainframeResponseContainer::TakeBody() {
  DCHECK(body_);
  return std::move(body_);
}
