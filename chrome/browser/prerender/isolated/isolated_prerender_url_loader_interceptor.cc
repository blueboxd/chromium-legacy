// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader_interceptor.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"

namespace {

bool ShouldInterceptRequestForPrerender(
    int frame_tree_node_id,
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(features::kIsolatePrerenders))
    return false;

  // TODO(crbug.com/1023486): Add other triggering checks.

  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return false;

  DCHECK_EQ(web_contents->GetBrowserContext(), browser_context);

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(browser_context);
  if (!prerender_manager)
    return false;

  return prerender_manager->IsWebContentsPrerendering(web_contents, nullptr);
}

}  // namespace

IsolatedPrerenderURLLoaderInterceptor::IsolatedPrerenderURLLoaderInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

IsolatedPrerenderURLLoaderInterceptor::
    ~IsolatedPrerenderURLLoaderInterceptor() = default;

void IsolatedPrerenderURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool intercept_prerender = ShouldInterceptRequestForPrerender(
      frame_tree_node_id_, tentative_resource_request, browser_context);

  if (intercept_prerender) {
    std::unique_ptr<IsolatedPrerenderURLLoader> url_loader =
        std::make_unique<IsolatedPrerenderURLLoader>(
            tentative_resource_request,
            content::BrowserContext::GetDefaultStoragePartition(browser_context)
                ->GetURLLoaderFactoryForBrowserProcess(),
            frame_tree_node_id_, 0 /* request_id */);
    std::move(callback).Run(url_loader->ServingResponseHandler());
    // url_loader manages its own lifetime once bound to the mojo pipes.
    url_loader.release();
    return;
  }

  std::move(callback).Run({});
}
