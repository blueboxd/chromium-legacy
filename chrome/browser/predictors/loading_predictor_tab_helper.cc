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
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

using content::BrowserThread;

namespace predictors {

namespace {

// Called only for subresources.
net::RequestPriority GetRequestPriority(
    network::mojom::RequestDestination request_destination) {
  switch (request_destination) {
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kFont:
      return net::HIGHEST;

    case network::mojom::RequestDestination::kScript:
      return net::MEDIUM;

    case network::mojom::RequestDestination::kEmpty:
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kDocument:
    case network::mojom::RequestDestination::kEmbed:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kImage:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kObject:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kReport:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kXslt:
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
    network::mojom::RequestDestination request_destination) {
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
  resource_load_info.request_destination = request_destination;
  resource_load_info.method = "GET";
  resource_load_info.request_priority =
      GetRequestPriority(resource_load_info.request_destination);
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
