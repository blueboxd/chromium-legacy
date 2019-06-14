// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/resource_loading_hints/resource_loading_hints_web_contents_observer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/previews/previews_content_util.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"
#include "url/gurl.h"

ResourceLoadingHintsWebContentsObserver::
    ~ResourceLoadingHintsWebContentsObserver() {}

ResourceLoadingHintsWebContentsObserver::
    ResourceLoadingHintsWebContentsObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

void ResourceLoadingHintsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage()) {
    return;
  }

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  if (!ui_tab_helper)
    return;

  previews::PreviewsUserData* previews_user_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);

  if (!previews_user_data ||
      previews_user_data->committed_previews_type() !=
          previews::PreviewsType::RESOURCE_LOADING_HINTS) {
    return;
  }
  // The committed previews type can sometimes not be cleared out if there were
  // no pre-commit previews allowed, so make sure we are not in the coin flip
  // holdback before proceeding with sending resource loading hints.
  if (previews_user_data->coin_flip_holdback_result() ==
      previews::CoinFlipHoldbackResult::kHoldback) {
    return;
  }

  DCHECK(previews::params::IsResourceLoadingHintsEnabled());
  SendResourceLoadingHints(navigation_handle,
                           previews_user_data->is_redirect());
}

void ResourceLoadingHintsWebContentsObserver::SendResourceLoadingHints(
    content::NavigationHandle* navigation_handle,
    bool is_redirect) const {
  // Hints should be sent only after the renderer frame has committed.
  DCHECK(navigation_handle->HasCommitted());
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(navigation_handle->GetURL().SchemeIsHTTPOrHTTPS());

  blink::mojom::PreviewsResourceLoadingHintsReceiverPtr hints_receiver_ptr;
  web_contents()->GetMainFrame()->GetRemoteInterfaces()->GetInterface(
      &hints_receiver_ptr);
  blink::mojom::PreviewsResourceLoadingHintsPtr hints_ptr =
      blink::mojom::PreviewsResourceLoadingHints::New();

  const std::vector<std::string>& hints =
      GetResourceLoadingHintsResourcePatternsToBlock(
          navigation_handle->GetURL());

  UMA_HISTOGRAM_BOOLEAN(
      "ResourceLoadingHints.ResourcePatternsAvailableAtCommit", !hints.empty());
  if (is_redirect) {
    UMA_HISTOGRAM_BOOLEAN(
        "ResourceLoadingHints.ResourcePatternsAvailableAtCommitForRedirect",
        !hints.empty());
  }

  if (hints.empty())
    return;

  hints_ptr->ukm_source_id = ukm::ConvertToSourceId(
      navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  for (const std::string& hint : hints)
    hints_ptr->subresources_to_block.push_back(hint);

  hints_receiver_ptr->SetResourceLoadingHints(std::move(hints_ptr));
}

const std::vector<std::string> ResourceLoadingHintsWebContentsObserver::
    GetResourceLoadingHintsResourcePatternsToBlock(
        const GURL& document_gurl) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(profile_);

  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(profile_);
  previews::PreviewsUIService* previews_ui_service =
      previews_service->previews_ui_service();
  return previews_ui_service->GetResourceLoadingHintsResourcePatternsToBlock(
      document_gurl);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ResourceLoadingHintsWebContentsObserver)
