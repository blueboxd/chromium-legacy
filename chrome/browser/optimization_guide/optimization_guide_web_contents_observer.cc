// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"

#include "base/base64.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

OptimizationGuideWebContentsObserver::OptimizationGuideWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide_keyed_service_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

OptimizationGuideWebContentsObserver::~OptimizationGuideWebContentsObserver() =
    default;

void OptimizationGuideWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  if (!optimization_guide_keyed_service_)
    return;

  optimization_guide_keyed_service_->MaybeLoadHintForNavigation(
      navigation_handle);
}

void OptimizationGuideWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  if (!optimization_guide_keyed_service_)
    return;

  optimization_guide_keyed_service_->MaybeLoadHintForNavigation(
      navigation_handle);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OptimizationGuideWebContentsObserver)
