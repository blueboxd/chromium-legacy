// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/prefetched_response_container.h"

PrefetchedResponseContainer::PrefetchedResponseContainer(
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<std::string> body)
    : head_(std::move(head)), body_(std::move(body)) {}

PrefetchedResponseContainer::~PrefetchedResponseContainer() = default;

network::mojom::URLResponseHeadPtr PrefetchedResponseContainer::TakeHead() {
  DCHECK(head_);
  return std::move(head_);
}

std::unique_ptr<std::string> PrefetchedResponseContainer::TakeBody() {
  DCHECK(body_);
  return std::move(body_);
}
