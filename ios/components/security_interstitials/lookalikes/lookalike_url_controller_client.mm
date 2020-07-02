// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_controller_client.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#include "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Creates a metrics helper for |url|.
std::unique_ptr<security_interstitials::IOSBlockingPageMetricsHelper>
CreateMetricsHelper(web::WebState* web_state, const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix = "lookalike";
  return std::make_unique<security_interstitials::IOSBlockingPageMetricsHelper>(
      web_state, url, reporting_info);
}
}  // namespace

LookalikeUrlControllerClient::LookalikeUrlControllerClient(
    web::WebState* web_state,
    const GURL& safe_url,
    const GURL& request_url,
    const std::string& app_locale)
    : IOSBlockingPageControllerClient(
          web_state,
          CreateMetricsHelper(web_state, request_url),
          app_locale),
      safe_url_(safe_url),
      request_url_(request_url),
      weak_factory_(this) {}

LookalikeUrlControllerClient::~LookalikeUrlControllerClient() {}

void LookalikeUrlControllerClient::GoBack() {
  // Instead of a 'go back' option, redirect to the legitimate site.
  OpenUrlInCurrentTab(safe_url_);
}

void LookalikeUrlControllerClient::Proceed() {
  LookalikeUrlTabAllowList::FromWebState(web_state())
      ->AllowDomain(request_url_.host());
  Reload();
}

void LookalikeUrlControllerClient::Close() {
  // Closing the tab synchronously is problematic since web state is heavily
  // involved in the operation and CloseWebState interrupts it, so call
  // CloseWebState asynchronously.
  base::PostTask(FROM_HERE, {web::WebThread::UI},
                 base::BindOnce(&IOSBlockingPageControllerClient::Close,
                                weak_factory_.GetWeakPtr()));
}
