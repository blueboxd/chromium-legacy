// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/background_tab_loading_policy.h"

#include "chrome/browser/performance_manager/mechanisms/page_loader.h"
#include "components/performance_manager/public/decorators/tab_properties_decorator.h"
#include "components/performance_manager/public/graph/policies/background_tab_loading_policy.h"
#include "components/performance_manager/public/performance_manager.h"

namespace performance_manager {

namespace policies {

namespace {
BackgroundTabLoadingPolicy* g_background_tab_loading_policy = nullptr;
}  // namespace

void ScheduleLoadForRestoredTabs(
    std::vector<content::WebContents*> web_contents_vector) {
  std::vector<base::WeakPtr<PageNode>> weakptr_page_nodes;
  weakptr_page_nodes.reserve(web_contents_vector.size());
  for (auto* content : web_contents_vector) {
    weakptr_page_nodes.push_back(
        PerformanceManager::GetPageNodeForWebContents(content));
  }
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](std::vector<base::WeakPtr<PageNode>> weakptr_page_nodes,
                        performance_manager::Graph* graph) {
                       std::vector<PageNode*> page_nodes;
                       page_nodes.reserve(weakptr_page_nodes.size());
                       for (auto page_node : weakptr_page_nodes) {
                         // If the PageNode has been deleted before
                         // BackgroundTabLoading starts restoring it, then there
                         // is no need to restore it.
                         if (PageNode* raw_page = page_node.get())
                           page_nodes.push_back(raw_page);
                       }
                       BackgroundTabLoadingPolicy::GetInstance()
                           ->ScheduleLoadForRestoredTabs(std::move(page_nodes));
                     },
                     std::move(weakptr_page_nodes)));
}

BackgroundTabLoadingPolicy::BackgroundTabLoadingPolicy()
    : page_loader_(std::make_unique<mechanism::PageLoader>()) {
  DCHECK(!g_background_tab_loading_policy);
  g_background_tab_loading_policy = this;
}

BackgroundTabLoadingPolicy::~BackgroundTabLoadingPolicy() {
  DCHECK_EQ(this, g_background_tab_loading_policy);
  g_background_tab_loading_policy = nullptr;
}

void BackgroundTabLoadingPolicy::OnPassedToGraph(Graph* graph) {}

void BackgroundTabLoadingPolicy::OnTakenFromGraph(Graph* graph) {}

void BackgroundTabLoadingPolicy::ScheduleLoadForRestoredTabs(
    std::vector<PageNode*> page_nodes) {
  for (auto* page_node : page_nodes) {
    DCHECK(
        TabPropertiesDecorator::Data::FromPageNode(page_node)->IsInTabStrip());
    page_loader_->LoadPageNode(page_node);
  }
}

void BackgroundTabLoadingPolicy::SetMockLoaderForTesting(
    std::unique_ptr<mechanism::PageLoader> loader) {
  page_loader_ = std::move(loader);
}

BackgroundTabLoadingPolicy* BackgroundTabLoadingPolicy::GetInstance() {
  return g_background_tab_loading_policy;
}

}  // namespace policies

}  // namespace performance_manager
