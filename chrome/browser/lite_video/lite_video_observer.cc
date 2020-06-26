// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_observer.h"

#include "base/metrics/histogram_macros_local.h"
#include "base/optional.h"
#include "chrome/browser/lite_video/lite_video_decider.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_keyed_service.h"
#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"

namespace {

// Returns the LiteVideoDecider when the LiteVideo features is enabled.
lite_video::LiteVideoDecider* GetLiteVideoDeciderFromWebContents(
    content::WebContents* web_contents) {
  DCHECK(lite_video::features::IsLiteVideoEnabled());
  if (!web_contents)
    return nullptr;

  if (Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
    return LiteVideoKeyedServiceFactory::GetForProfile(profile)
        ->lite_video_decider();
  }
  return nullptr;
}

}  // namespace

// static
void LiteVideoObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (lite_video::features::IsLiteVideoEnabled())
    LiteVideoObserver::CreateForWebContents(web_contents);
}

LiteVideoObserver::LiteVideoObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  lite_video_decider_ = GetLiteVideoDeciderFromWebContents(web_contents);
}

LiteVideoObserver::~LiteVideoObserver() = default;

void LiteVideoObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  if (!lite_video_decider_)
    return;

  base::Optional<lite_video::LiteVideoHint> hint =
      lite_video_decider_->CanApplyLiteVideo(navigation_handle);

  LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.Navigation.HasHint", hint ? true : false);

  // TODO(crbug/1082553): Add logic to pass the hint via the
  // ResourceLoadingAgent to the LiteVideoAgent for use when throttling media
  // requests.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LiteVideoObserver)
