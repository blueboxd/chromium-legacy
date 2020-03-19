// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_contents_observer.h"

#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/origin.h"

MediaFeedsContentsObserver::MediaFeedsContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

MediaFeedsContentsObserver::~MediaFeedsContentsObserver() = default;

void MediaFeedsContentsObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame() || handle->IsSameDocument())
    return;

  render_frame_.reset();
}

void MediaFeedsContentsObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->GetParent() || !GetService())
    return;

  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &render_frame_);

  // Unretained is safe here because MediaFeedsContentsObserver owns the mojo
  // remote.
  render_frame_->GetMediaFeedURL(base::BindOnce(
      &MediaFeedsContentsObserver::DidFindMediaFeed, base::Unretained(this),
      render_frame_host->GetLastCommittedOrigin()));
}

void MediaFeedsContentsObserver::DidFindMediaFeed(
    const url::Origin& origin,
    const base::Optional<GURL>& url) {
  auto* service = GetService();
  if (!service)
    return;

  // The feed should be the same origin as the original page that had the feed
  // on it.
  if (url) {
    if (!origin.IsSameOriginWith(url::Origin::Create(*url))) {
      mojo::ReportBadMessage(
          "GetMediaFeedURL. The URL should be the same origin has the page.");
      return;
    }

    service->DiscoverMediaFeed(*url);
  }

  if (test_closure_)
    std::move(test_closure_).Run();
}

media_history::MediaHistoryKeyedService*
MediaFeedsContentsObserver::GetService() {
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  return media_history::MediaHistoryKeyedServiceFactory::GetForProfile(profile);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaFeedsContentsObserver)
