// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_tab_helper.h"

#include <string>

#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

using content::BrowserThread;

namespace predictors {

namespace {

net::RequestPriority GetRequestPriority(
    blink::mojom::ResourceType resource_type) {
  switch (resource_type) {
    case blink::mojom::ResourceType::kMainFrame:
    case blink::mojom::ResourceType::kStylesheet:
    case blink::mojom::ResourceType::kFontResource:
      return net::HIGHEST;
    case blink::mojom::ResourceType::kScript:
      return net::MEDIUM;
    case blink::mojom::ResourceType::kSubFrame:
    case blink::mojom::ResourceType::kImage:
    case blink::mojom::ResourceType::kSubResource:
    case blink::mojom::ResourceType::kObject:
    case blink::mojom::ResourceType::kMedia:
    case blink::mojom::ResourceType::kWorker:
    case blink::mojom::ResourceType::kSharedWorker:
    case blink::mojom::ResourceType::kPrefetch:
    case blink::mojom::ResourceType::kFavicon:
    case blink::mojom::ResourceType::kXhr:
    case blink::mojom::ResourceType::kPing:
    case blink::mojom::ResourceType::kServiceWorker:
    case blink::mojom::ResourceType::kCspReport:
    case blink::mojom::ResourceType::kPluginResource:
    case blink::mojom::ResourceType::kNavigationPreloadMainFrame:
    case blink::mojom::ResourceType::kNavigationPreloadSubFrame:
      return net::LOWEST;
  }
}

bool IsHandledNavigation(content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInMainFrame() &&
         !navigation_handle->IsSameDocument() &&
         navigation_handle->GetURL().SchemeIsHTTPOrHTTPS();
}

}  // namespace

LoadingPredictorTabHelper::LoadingPredictorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  auto* predictor = LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (predictor)
    predictor_ = predictor->GetWeakPtr();
}

LoadingPredictorTabHelper::~LoadingPredictorTabHelper() = default;

void LoadingPredictorTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  if (!IsHandledNavigation(navigation_handle))
    return;

  auto navigation_id = NavigationID(web_contents(), navigation_handle->GetURL(),
                                    navigation_handle->NavigationStart());
  if (!navigation_id.is_valid())
    return;

  predictor_->OnNavigationStarted(navigation_id);
}

void LoadingPredictorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  if (!IsHandledNavigation(navigation_handle))
    return;

  auto old_navigation_id = NavigationID(
      web_contents(), navigation_handle->GetRedirectChain().front(),
      navigation_handle->NavigationStart());
  auto new_navigation_id =
      NavigationID(web_contents(), navigation_handle->GetURL(),
                   navigation_handle->NavigationStart());
  if (!old_navigation_id.is_valid() || !new_navigation_id.is_valid())
    return;

  predictor_->OnNavigationFinished(old_navigation_id, new_navigation_id,
                                   navigation_handle->IsErrorPage());
}

void LoadingPredictorTabHelper::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  bool is_main_frame = render_frame_host->GetParent() == nullptr;
  if (!is_main_frame)
    return;

  auto navigation_id = NavigationID(web_contents());
  if (!navigation_id.is_valid())
    return;

  predictor_->loading_data_collector()->RecordResourceLoadComplete(
      navigation_id, resource_load_info);
}

void LoadingPredictorTabHelper::DidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& mime_type,
    blink::mojom::ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  auto navigation_id = NavigationID(web_contents());
  if (!navigation_id.is_valid())
    return;

  blink::mojom::ResourceLoadInfo resource_load_info;
  resource_load_info.original_url = url;
  resource_load_info.final_url = url;
  resource_load_info.mime_type = mime_type;
  resource_load_info.resource_type = resource_type;
  resource_load_info.method = "GET";
  resource_load_info.request_priority =
      GetRequestPriority(resource_load_info.resource_type);
  resource_load_info.network_info =
      blink::mojom::CommonNetworkInfo::New(false, false, base::nullopt);
  predictor_->loading_data_collector()->RecordResourceLoadComplete(
      navigation_id, resource_load_info);
}

void LoadingPredictorTabHelper::DocumentOnLoadCompletedInMainFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  auto navigation_id = NavigationID(web_contents());
  if (!navigation_id.is_valid())
    return;

  predictor_->loading_data_collector()->RecordMainFrameLoadComplete(
      navigation_id);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoadingPredictorTabHelper)

}  // namespace predictors
