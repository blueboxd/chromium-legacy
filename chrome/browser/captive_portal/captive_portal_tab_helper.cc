// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/captive_portal/content/captive_portal_login_detector.h"
#include "components/captive_portal/content/captive_portal_tab_reloader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_config.h"
#include "net/ssl/ssl_info.h"

using captive_portal::CaptivePortalResult;

CaptivePortalTabHelper::CaptivePortalTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      navigation_handle_(nullptr),
      tab_reloader_(new CaptivePortalTabReloader(
          CaptivePortalServiceFactory::GetForProfile(profile_),
          web_contents,
          base::Bind(&CaptivePortalTabHelper::OpenLoginTabForWebContents,
                     web_contents,
                     false))),
      login_detector_(new CaptivePortalLoginDetector(
          CaptivePortalServiceFactory::GetForProfile(profile_))),
      is_captive_portal_window_(false),
      subscription_(
          CaptivePortalServiceFactory::GetForProfile(profile_)
              ->RegisterCallback(base::Bind(&CaptivePortalTabHelper::Observe,
                                            base::Unretained(this)))) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CaptivePortalTabHelper::~CaptivePortalTabHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CaptivePortalTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // TODO(clamy): The root cause behind crbug.com/704892 is known.
  // Remove this code if it is never reached until ~ 2017-July-20.
  if (navigation_handle == navigation_handle_)
    base::debug::DumpWithoutCrashing();

  bool was_tracking_navigation = !!navigation_handle_;
  navigation_handle_ = navigation_handle;

  // Always track the latest navigation. If a navigation was already tracked,
  // and it committed (either the navigation proper or an error page), it is
  // safe to start tracking the new navigation. Otherwise simulate an abort
  // before reporting the start of the new navigation.
  if (was_tracking_navigation)
    tab_reloader_->OnAbort();

  tab_reloader_->OnLoadStart(
      navigation_handle->GetURL().SchemeIsCryptographic());
}

void CaptivePortalTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (navigation_handle != navigation_handle_)
    return;
  DCHECK(navigation_handle->IsInMainFrame());
  tab_reloader_->OnRedirect(
      navigation_handle->GetURL().SchemeIsCryptographic());
}

void CaptivePortalTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Exclude subframe navigations.
  if (!navigation_handle->IsInMainFrame())
    return;

  // Exclude same-document navigations and aborted navigations that were not
  // being tracked.
  if (navigation_handle_ != navigation_handle &&
      (!navigation_handle->HasCommitted() ||
       navigation_handle->IsSameDocument())) {
    return;
  }

  bool need_to_simulate_start = navigation_handle_ != navigation_handle;
  bool need_to_simulate_previous_abort =
      need_to_simulate_start && !!navigation_handle_;
  navigation_handle_ = nullptr;

  if (need_to_simulate_previous_abort)
    tab_reloader_->OnAbort();

  if (need_to_simulate_start) {
    tab_reloader_->OnLoadStart(
        navigation_handle->GetURL().SchemeIsCryptographic());
  }

  if (navigation_handle->HasCommitted()) {
    tab_reloader_->OnLoadCommitted(navigation_handle->GetNetErrorCode());
  } else {
    tab_reloader_->OnAbort();
  }
}

void CaptivePortalTabHelper::DidStopLoading() {
  login_detector_->OnStoppedLoading();
}

void CaptivePortalTabHelper::Observe(
    const CaptivePortalService::Results& results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnCaptivePortalResults(results.previous_result, results.result);
}

void CaptivePortalTabHelper::OnSSLCertError(const net::SSLInfo& ssl_info) {
  tab_reloader_->OnSSLCertError(ssl_info);
}

bool CaptivePortalTabHelper::IsLoginTab() const {
  return login_detector_->is_login_tab();
}

// static
void CaptivePortalTabHelper::OpenLoginTabForWebContents(
    content::WebContents* web_contents,
    bool focus) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

  // If the Profile doesn't have a tabbed browser window open, do nothing.
  if (!browser)
    return;

  bool insecure_stub_resolver_enabled;
  net::DnsConfig::SecureDnsMode secure_dns_mode;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers;
  SystemNetworkContextManager::GetStubResolverConfig(
      g_browser_process->local_state(), &insecure_stub_resolver_enabled,
      &secure_dns_mode, &dns_over_https_servers);

  // If the DNS mode is SECURE, captive portal login tabs should be opened in
  // new popup windows where secure DNS will be disabled.
  if (secure_dns_mode == net::DnsConfig::SecureDnsMode::SECURE) {
    // If there is already a captive portal popup window, do not create another.
    for (auto* contents : AllTabContentses()) {
      CaptivePortalTabHelper* captive_portal_tab_helper =
          CaptivePortalTabHelper::FromWebContents(contents);
      if (captive_portal_tab_helper->IsLoginTab()) {
        Browser* browser_with_login_tab =
            chrome::FindBrowserWithWebContents(contents);
        browser_with_login_tab->window()->Show();
        browser_with_login_tab->tab_strip_model()->ActivateTabAt(
            browser_with_login_tab->tab_strip_model()->GetIndexOfWebContents(
                contents));
        return;
      }
    }

    // Otherwise, create a captive portal popup window.
    NavigateParams params(
        browser,
        CaptivePortalServiceFactory::GetForProfile(browser->profile())
            ->test_url(),
        ui::PAGE_TRANSITION_TYPED);
    params.disposition = WindowOpenDisposition::NEW_POPUP;
    Navigate(&params);
    content::WebContents* new_contents = params.navigated_or_inserted_contents;
    CaptivePortalTabHelper* captive_portal_tab_helper =
        CaptivePortalTabHelper::FromWebContents(new_contents);
    captive_portal_tab_helper->set_is_captive_portal_window();
    captive_portal_tab_helper->SetIsLoginTab();
    return;
  }

  // Check if the Profile's topmost browser window already has a login tab.
  // If so, do nothing.
  // TODO(mmenke):  Consider focusing that tab, at least if this is the tab
  //                helper for the currently active tab for the profile.
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    content::WebContents* contents =
        browser->tab_strip_model()->GetWebContentsAt(i);
    CaptivePortalTabHelper* captive_portal_tab_helper =
        CaptivePortalTabHelper::FromWebContents(contents);
    if (captive_portal_tab_helper->IsLoginTab()) {
      if (focus)
        browser->tab_strip_model()->ActivateTabAt(i);
      return;
    }
  }

  // Otherwise, open a login tab.  Only end up here when a captive portal result
  // was received, so it's safe to assume profile has a CaptivePortalService.
  content::WebContents* new_contents = chrome::AddSelectedTabWithURL(
      browser,
      CaptivePortalServiceFactory::GetForProfile(
          browser->profile())->test_url(),
      ui::PAGE_TRANSITION_TYPED);
  CaptivePortalTabHelper* captive_portal_tab_helper =
      CaptivePortalTabHelper::FromWebContents(new_contents);
  captive_portal_tab_helper->SetIsLoginTab();
}

void CaptivePortalTabHelper::OnCaptivePortalResults(
    CaptivePortalResult previous_result,
    CaptivePortalResult result) {
  tab_reloader_->OnCaptivePortalResults(previous_result, result);
  login_detector_->OnCaptivePortalResults(previous_result, result);
}

void CaptivePortalTabHelper::SetIsLoginTab() {
  login_detector_->SetIsLoginTab();
}

void CaptivePortalTabHelper::SetTabReloaderForTest(
    CaptivePortalTabReloader* tab_reloader) {
  tab_reloader_.reset(tab_reloader);
}

CaptivePortalTabReloader* CaptivePortalTabHelper::GetTabReloaderForTest() {
  return tab_reloader_.get();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CaptivePortalTabHelper)
