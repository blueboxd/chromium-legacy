// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/background_tab_loading_policy.h"

#include "chrome/browser/performance_manager/mechanisms/page_loader.h"

namespace performance_manager {

namespace policies {

BackgroundTabLoadingPolicy::BackgroundTabLoadingPolicy()
    : page_loader_(std::make_unique<mechanism::PageLoader>()) {}
BackgroundTabLoadingPolicy::~BackgroundTabLoadingPolicy() = default;

void BackgroundTabLoadingPolicy::OnPassedToGraph(Graph* graph) {}

void BackgroundTabLoadingPolicy::OnTakenFromGraph(Graph* graph) {}

void BackgroundTabLoadingPolicy::RestoreTabs(
    std::vector<PageNode*> page_nodes) {
  // TODO(https://crbug.com/1059341): DCHECK that |page_node| is in a tab strip.
  for (auto* page_node : page_nodes) {
    page_loader_->LoadPageNode(page_node);
  }
}

void BackgroundTabLoadingPolicy::SetMockLoaderForTesting(
    std::unique_ptr<mechanism::PageLoader> loader) {
  page_loader_ = std::move(loader);
}

}  // namespace policies

}  // namespace performance_manager
