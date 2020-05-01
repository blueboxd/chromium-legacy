// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/triggers/suspicious_site_trigger.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/features.h"

namespace safe_browsing {
namespace {

// Destroys the prerender contents associated with the web_contents, if any.
void DestroyPrerenderContents(
    content::WebContents::OnceGetter web_contents_getter) {
  content::WebContents* web_contents = std::move(web_contents_getter).Run();
  if (web_contents) {
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents)
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
  }
}

void CreateSafeBrowsingUserInteractionObserver(
    const content::WebContents::Getter& web_contents_getter,
    const security_interstitials::UnsafeResource& resource,
    bool is_main_frame,
    scoped_refptr<SafeBrowsingUIManager> ui_manager) {
  content::WebContents* web_contents = web_contents_getter.Run();
  // Don't delay the interstitial for prerender pages.
  if (prerender::PrerenderContents::FromWebContents(web_contents)) {
    SafeBrowsingUIManager::StartDisplayingBlockingPage(ui_manager, resource);
    return;
  }
  SafeBrowsingUserInteractionObserver::CreateForWebContents(
      web_contents, resource, is_main_frame, ui_manager);
}

}  // namespace

UrlCheckerDelegateImpl::UrlCheckerDelegateImpl(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : database_manager_(std::move(database_manager)),
      ui_manager_(std::move(ui_manager)),
      threat_types_(CreateSBThreatTypeSet({
// TODO(crbug.com/835961): Enable on Android when list is available.
#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
        safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE,
#endif
            safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
            safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
            safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
            safe_browsing::SB_THREAT_TYPE_BILLING
      })) {
}

UrlCheckerDelegateImpl::~UrlCheckerDelegateImpl() = default;

void UrlCheckerDelegateImpl::MaybeDestroyPrerenderContents(
    content::WebContents::OnceGetter web_contents_getter) {
  // Destroy the prefetch with FINAL_STATUS_SAFEBROSWING.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&DestroyPrerenderContents,
                                std::move(web_contents_getter)));
}

void UrlCheckerDelegateImpl::StartDisplayingBlockingPageHelper(
    const security_interstitials::UnsafeResource& resource,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    bool is_main_frame,
    bool has_user_gesture) {
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&SafeBrowsingUIManager::StartDisplayingBlockingPage,
                     ui_manager_, resource));
}

// Starts displaying the SafeBrowsing interstitial page.
void UrlCheckerDelegateImpl::
    StartObservingInteractionsForDelayedBlockingPageHelper(
        const security_interstitials::UnsafeResource& resource,
        bool is_main_frame) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&CreateSafeBrowsingUserInteractionObserver,
                                resource.web_contents_getter, resource,
                                is_main_frame, ui_manager_));
}

bool UrlCheckerDelegateImpl::IsUrlWhitelisted(const GURL& url) {
  return false;
}

bool UrlCheckerDelegateImpl::ShouldSkipRequestCheck(
    const GURL& original_url,
    int frame_tree_node_id,
    int render_process_id,
    int render_frame_id,
    bool originated_from_service_worker) {
  return false;
}

void UrlCheckerDelegateImpl::NotifySuspiciousSiteDetected(
    const base::RepeatingCallback<content::WebContents*()>&
        web_contents_getter) {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&NotifySuspiciousSiteTriggerDetected,
                                web_contents_getter));
}

const SBThreatTypeSet& UrlCheckerDelegateImpl::GetThreatTypes() {
  return threat_types_;
}

SafeBrowsingDatabaseManager* UrlCheckerDelegateImpl::GetDatabaseManager() {
  return database_manager_.get();
}

BaseUIManager* UrlCheckerDelegateImpl::GetUIManager() {
  return ui_manager_.get();
}

}  // namespace safe_browsing
