// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/url_checker_delegate_impl.h"

#include "components/safe_browsing/db/database_manager.h"
#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

namespace weblayer {

UrlCheckerDelegateImpl::UrlCheckerDelegateImpl(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : database_manager_(std::move(database_manager)),
      ui_manager_(std::move(ui_manager)) {}

UrlCheckerDelegateImpl::~UrlCheckerDelegateImpl() = default;

void UrlCheckerDelegateImpl::MaybeDestroyPrerenderContents(
    const base::Callback<content::WebContents*()>& web_contents_getter) {}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_main_frame,
    bool has_user_gesture) {
  // TODO(timvolodine): figure out what to do here.
}

bool UrlCheckerDelegateImpl::IsUrlWhitelisted(const GURL& url) {
  // TODO(timvolodine): false for now, we may want whitelisting support later.
  return false;
}

bool UrlCheckerDelegateImpl::ShouldSkipRequestCheck(
    content::ResourceContext* resource_context,
    const GURL& original_url,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id,
    bool originated_from_service_worker) {
  // TODO(timvolodine): this is needed when safebrowsing is not enabled.
  // For now in the context of weblayer we consider safebrowsing as always
  // enabled. This may change in the future.
  return false;
}

void UrlCheckerDelegateImpl::NotifySuspiciousSiteDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {}

const safe_browsing::SBThreatTypeSet& UrlCheckerDelegateImpl::GetThreatTypes() {
  // TODO(timvolodine): revisit with the relevant threat types.
  return threat_types_;
}

safe_browsing::SafeBrowsingDatabaseManager*
UrlCheckerDelegateImpl::GetDatabaseManager() {
  return database_manager_.get();
}

safe_browsing::BaseUIManager* UrlCheckerDelegateImpl::GetUIManager() {
  return ui_manager_.get();
}

}  // namespace weblayer
