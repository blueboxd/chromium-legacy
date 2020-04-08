// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

const char kRestoreNavigationItemCount[] = "IOS.RestoreNavigationItemCount";

NavigationManager::WebLoadParams::WebLoadParams(const GURL& url)
    : url(url),
      transition_type(ui::PAGE_TRANSITION_LINK),
      user_agent_override_option(UserAgentOverrideOption::INHERIT),
      is_renderer_initiated(false),
      post_data(nil) {}

NavigationManager::WebLoadParams::~WebLoadParams() {}

NavigationManager::WebLoadParams::WebLoadParams(const WebLoadParams& other)
    : url(other.url),
      virtual_url(other.virtual_url),
      referrer(other.referrer),
      transition_type(other.transition_type),
      user_agent_override_option(other.user_agent_override_option),
      is_renderer_initiated(other.is_renderer_initiated),
      extra_headers([other.extra_headers copy]),
      post_data([other.post_data copy]) {}

NavigationManager::WebLoadParams& NavigationManager::WebLoadParams::operator=(
    const WebLoadParams& other) {
  url = other.url;
  virtual_url = other.virtual_url;
  referrer = other.referrer;
  is_renderer_initiated = other.is_renderer_initiated;
  transition_type = other.transition_type;
  user_agent_override_option = other.user_agent_override_option;
  extra_headers = [other.extra_headers copy];
  post_data = [other.post_data copy];

  return *this;
}

/* static */
NavigationItem* NavigationManagerImpl::GetLastCommittedNonRedirectedItem(
    const NavigationManager* nav_manager) {
  if (!nav_manager || !nav_manager->GetItemCount())
    return nullptr;

  int index = nav_manager->GetLastCommittedItemIndex();
  while (index >= 0) {
    NavigationItem* item = nav_manager->GetItemAtIndex(index);
    // Returns the first non-Redirect item found.
    if (!ui::PageTransitionIsRedirect(item->GetTransitionType()))
      return item;
    --index;
  }

  return nullptr;
}

void NavigationManagerImpl::UpdatePendingItemUserAgentType(
    UserAgentOverrideOption user_agent_override_option,
    const NavigationItem* inherit_from_item,
    NavigationItem* pending_item) {
  DCHECK(pending_item);

  // |user_agent_override_option| must be INHERIT if |pending_item|'s
  // UserAgentType is NONE, as requesting a desktop or mobile user agent should
  // be disabled for app-specific URLs.
  DCHECK(pending_item->GetUserAgentForInheritance() != UserAgentType::NONE ||
         user_agent_override_option == UserAgentOverrideOption::INHERIT);

  // Newly created pending items are created with UserAgentType::NONE for native
  // pages or UserAgentType::MOBILE or UserAgentType::DESKTOP for non-native
  // pages.  If the pending item's URL is non-native, check which user agent
  // type it should be created with based on |user_agent_override_option|.
  if (pending_item->GetUserAgentForInheritance() == UserAgentType::NONE)
    return;

  switch (user_agent_override_option) {
    case UserAgentOverrideOption::DESKTOP:
      pending_item->SetUserAgentType(UserAgentType::DESKTOP);
      break;
    case UserAgentOverrideOption::MOBILE:
      pending_item->SetUserAgentType(UserAgentType::MOBILE);
      break;
    case UserAgentOverrideOption::INHERIT: {
      // Propagate the last committed non-native item's UserAgentType if there
      // is one, otherwise keep the default value, which is mobile.
      DCHECK(!inherit_from_item ||
             inherit_from_item->GetUserAgentForInheritance() !=
                 UserAgentType::NONE);
      if (inherit_from_item) {
        pending_item->SetUserAgentType(
            inherit_from_item->GetUserAgentForInheritance());
      }
      break;
    }
  }
}

NavigationManagerImpl::NavigationManagerImpl()
    : delegate_(nullptr), browser_state_(nullptr) {}

NavigationManagerImpl::~NavigationManagerImpl() = default;

void NavigationManagerImpl::SetDelegate(NavigationManagerDelegate* delegate) {
  delegate_ = delegate;
}

void NavigationManagerImpl::SetBrowserState(BrowserState* browser_state) {
  browser_state_ = browser_state;
}

void NavigationManagerImpl::DetachFromWebView() {}

void NavigationManagerImpl::ApplyWKWebViewForwardHistoryClobberWorkaround() {
  NOTREACHED();
}

void NavigationManagerImpl::SetWKWebViewNextPendingUrlNotSerializable(
    const GURL& url) {
  NOTREACHED();
}

void NavigationManagerImpl::RemoveTransientURLRewriters() {
  transient_url_rewriters_.clear();
}

void NavigationManagerImpl::UpdatePendingItemUrl(const GURL& url) const {
  // If there is no pending item, navigation is probably happening within the
  // back forward history. Don't modify the item list.
  NavigationItemImpl* pending_item = GetPendingItemInCurrentOrRestoredSession();
  if (!pending_item || url == pending_item->GetURL())
    return;

  // UpdatePendingItemUrl is used to handle redirects after loading starts for
  // the currenting pending item. No transient item should exists at this point.
  DCHECK(!GetTransientItem());
  pending_item->SetURL(url);
  pending_item->SetVirtualURL(url);
  // Redirects (3xx response code), or client side navigation must change POST
  // requests to GETs.
  pending_item->SetPostData(nil);
  pending_item->ResetHttpRequestHeaders();
}

NavigationItemImpl* NavigationManagerImpl::GetCurrentItemImpl() const {
  NavigationItemImpl* transient_item = GetTransientItemImpl();
  if (transient_item)
    return transient_item;

  NavigationItemImpl* pending_item = GetPendingItemInCurrentOrRestoredSession();
  if (pending_item)
    return pending_item;

  return GetLastCommittedItemInCurrentOrRestoredSession();
}

NavigationItemImpl* NavigationManagerImpl::GetLastCommittedItemImpl() const {
  // GetLastCommittedItemImpl() should return null while session restoration is
  // in progress and real item after the first post-restore navigation is
  // finished. IsRestoreSessionInProgress(), will return true until the first
  // post-restore is started.
  if (IsRestoreSessionInProgress())
    return nullptr;

  NavigationItemImpl* result = GetLastCommittedItemInCurrentOrRestoredSession();
  if (!result || wk_navigation_util::IsRestoreSessionUrl(result->GetURL())) {
    // Session restoration has completed, but the first post-restore navigation
    // has not finished yet, so there is no committed URLs in the navigation
    // stack.
    return nullptr;
  }

  return result;
}

void NavigationManagerImpl::UpdateCurrentItemForReplaceState(
    const GURL& url,
    NSString* state_object) {
  DCHECK(!GetTransientItem());
  NavigationItemImpl* current_item = GetCurrentItemImpl();
  current_item->SetURL(url);
  current_item->SetSerializedStateObject(state_object);
  current_item->SetHasStateBeenReplaced(true);
  current_item->SetPostData(nil);
}

void NavigationManagerImpl::GoToIndex(int index,
                                      NavigationInitiationType initiation_type,
                                      bool has_user_gesture) {
  if (index < 0 || index >= GetItemCount()) {
    NOTREACHED();
    return;
  }

  if (!GetTransientItem()) {
    delegate_->RecordPageStateInNavigationItem();
  }
  delegate_->ClearTransientContent();
  delegate_->ClearDialogs();

  FinishGoToIndex(index, initiation_type, has_user_gesture);
}

void NavigationManagerImpl::GoToIndex(int index) {
  // Silently return if still on a restore URL.  This state should only last a
  // few moments, but may be triggered when a user mashes the back or forward
  // button quickly.
  NavigationItemImpl* item = GetLastCommittedItemInCurrentOrRestoredSession();
  if (item && wk_navigation_util::IsRestoreSessionUrl(item->GetURL())) {
    return;
  }
  GoToIndex(index, NavigationInitiationType::BROWSER_INITIATED,
            /*has_user_gesture=*/true);
}

NavigationItem* NavigationManagerImpl::GetLastCommittedItem() const {
  return GetLastCommittedItemImpl();
}

int NavigationManagerImpl::GetLastCommittedItemIndex() const {
  // GetLastCommittedItemIndex() should return -1 while session restoration is
  // in progress and real item after the first post-restore navigation is
  // finished. IsRestoreSessionInProgress(), will return true until the first
  // post-restore is started.
  if (IsRestoreSessionInProgress())
    return -1;

  NavigationItem* item = GetLastCommittedItemInCurrentOrRestoredSession();
  if (!item || wk_navigation_util::IsRestoreSessionUrl(item->GetURL())) {
    // Session restoration has completed, but the first post-restore
    // navigation has not finished yet, so there is no committed URLs in the
    // navigation stack.
    return -1;
  }

  return GetLastCommittedItemIndexInCurrentOrRestoredSession();
}

NavigationItem* NavigationManagerImpl::GetPendingItem() const {
  if (IsRestoreSessionInProgress())
    return nullptr;
  return GetPendingItemInCurrentOrRestoredSession();
}

NavigationItem* NavigationManagerImpl::GetTransientItem() const {
  return GetTransientItemImpl();
}

void NavigationManagerImpl::LoadURLWithParams(
    const NavigationManager::WebLoadParams& params) {
  DCHECK(!(params.transition_type & ui::PAGE_TRANSITION_FORWARD_BACK));
  delegate_->ClearTransientContent();
  delegate_->ClearDialogs();
  delegate_->RecordPageStateInNavigationItem();

  NavigationInitiationType initiation_type =
      params.is_renderer_initiated
          ? NavigationInitiationType::RENDERER_INITIATED
          : NavigationInitiationType::BROWSER_INITIATED;
  AddPendingItem(params.url, params.referrer, params.transition_type,
                 initiation_type, params.user_agent_override_option);

  // Mark pending item as created from hash change if necessary. This is needed
  // because window.hashchange message may not arrive on time.
  NavigationItemImpl* pending_item = GetPendingItemInCurrentOrRestoredSession();
  if (pending_item) {
    NavigationItem* last_committed_item =
        GetLastCommittedItemInCurrentOrRestoredSession();
    GURL last_committed_url = last_committed_item
                                  ? last_committed_item->GetVirtualURL()
                                  : GURL::EmptyGURL();
    GURL pending_url = pending_item->GetURL();
    if (last_committed_url != pending_url &&
        last_committed_url.EqualsIgnoringRef(pending_url)) {
      pending_item->SetIsCreatedFromHashChange(true);
    }

    if (params.virtual_url.is_valid())
      pending_item->SetVirtualURL(params.virtual_url);
  }

  // Add additional headers to the NavigationItem before loading it in the web
  // view. This implementation must match CRWWebController's |currentNavItem|.
  // However, to avoid introducing a GetCurrentItem() that is only used here,
  // the logic in |currentNavItem| is inlined here with the small simplification
  // since AddPendingItem() implies that any transient item would have been
  // cleared.
  DCHECK(!GetTransientItem());
  NavigationItemImpl* added_item =
      pending_item ? pending_item
                   : GetLastCommittedItemInCurrentOrRestoredSession();
  DCHECK(added_item);
  if (params.extra_headers)
    added_item->AddHttpRequestHeaders(params.extra_headers);
  if (params.post_data) {
    DCHECK([added_item->GetHttpRequestHeaders() objectForKey:@"Content-Type"])
        << "Post data should have an associated content type";
    added_item->SetPostData(params.post_data);
    added_item->SetShouldSkipRepostFormConfirmation(true);
  }

  FinishLoadURLWithParams(initiation_type);
}

void NavigationManagerImpl::AddTransientURLRewriter(
    BrowserURLRewriter::URLRewriter rewriter) {
  DCHECK(rewriter);
  transient_url_rewriters_.push_back(rewriter);
}

void NavigationManagerImpl::Reload(ReloadType reload_type,
                                   bool check_for_reposts) {
  if (IsRestoreSessionInProgress()) {
    // Do not interrupt session restoration process. Last committed item will
    // eventually reload once the session is restored.
    return;
  }

  // Use GetLastCommittedItemInCurrentOrRestoredSession() instead of
  // GetLastCommittedItem() so restore session URL's aren't suppressed.
  // Otherwise a cancelled/stopped navigation during the first post-restore
  // navigation will always return early from Reload.
  if (!GetTransientItem() && !GetPendingItem() &&
      !GetLastCommittedItemInCurrentOrRestoredSession())
    return;

  delegate_->ClearDialogs();

  // Reload with ORIGINAL_REQUEST_URL type should reload with the original
  // request url of the transient item, or pending item if transient doesn't
  // exist, or last committed item if both of them don't exist. The reason is
  // that a server side redirect may change the item's url.
  // For example, the user visits www.chromium.org and is then redirected
  // to m.chromium.org, when the user wants to refresh the page with a different
  // configuration (e.g. user agent), the user would be expecting to visit
  // www.chromium.org instead of m.chromium.org.
  if (reload_type == web::ReloadType::ORIGINAL_REQUEST_URL) {
    NavigationItem* reload_item = nullptr;
    if (GetTransientItem())
      reload_item = GetTransientItem();
    else if (GetPendingItem())
      reload_item = GetPendingItem();
    else
      reload_item = GetLastCommittedItemInCurrentOrRestoredSession();
    DCHECK(reload_item);

    reload_item->SetURL(reload_item->GetOriginalRequestURL());
  }

  FinishReload();
}

void NavigationManagerImpl::ReloadWithUserAgentType(
    UserAgentType user_agent_type) {
  DCHECK_NE(user_agent_type, UserAgentType::NONE);

  NavigationItem* item_to_reload = GetTransientItem();
  if (!item_to_reload ||
      ui::PageTransitionIsRedirect(item_to_reload->GetTransitionType()))
    item_to_reload = GetVisibleItem();
  if (!item_to_reload ||
      ui::PageTransitionIsRedirect(item_to_reload->GetTransitionType())) {
    NavigationItem* last_committed_before_redirect =
        GetLastCommittedNonRedirectedItem(this);
    if (last_committed_before_redirect) {
      // When a tab is opened on a redirect, there is no last committed item
      // before the redirect. In that case, take the last committed item.
      item_to_reload = last_committed_before_redirect;
    }
  }

  if (!item_to_reload)
    return;

  // |reloadURL| will be empty if a page was open by DOM.
  GURL reload_url(item_to_reload->GetOriginalRequestURL());
  if (reload_url.is_empty()) {
    reload_url = item_to_reload->GetVirtualURL();
  }

  WebLoadParams params(reload_url);
  if (item_to_reload->GetVirtualURL() != reload_url)
    params.virtual_url = item_to_reload->GetVirtualURL();
  params.referrer = item_to_reload->GetReferrer();
  params.transition_type = ui::PAGE_TRANSITION_RELOAD;

  switch (user_agent_type) {
    case UserAgentType::DESKTOP:
      params.user_agent_override_option = UserAgentOverrideOption::DESKTOP;
      break;
    case UserAgentType::MOBILE:
      params.user_agent_override_option = UserAgentOverrideOption::MOBILE;
      break;
    case UserAgentType::AUTOMATIC:
    case UserAgentType::NONE:
      NOTREACHED();
  }

  LoadURLWithParams(params);
}

void NavigationManagerImpl::LoadIfNecessary() {
  delegate_->LoadIfNecessary();
}

void NavigationManagerImpl::AddRestoreCompletionCallback(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void NavigationManagerImpl::WillRestore(size_t item_count) {
  // It should be uncommon for the user to have more than 100 items in their
  // session, so bucketing 100+ logs together is fine.
  UMA_HISTOGRAM_COUNTS_100(kRestoreNavigationItemCount, item_count);
}

void NavigationManagerImpl::RewriteItemURLIfNecessary(
    NavigationItem* item) const {
  GURL url = item->GetURL();
  if (web::BrowserURLRewriter::GetInstance()->RewriteURLIfNecessary(
          &url, browser_state_)) {
    // |url| must be set first for -SetVirtualURL to not no-op.
    GURL virtual_url = item->GetURL();
    item->SetURL(url);
    item->SetVirtualURL(virtual_url);
  }
}

std::unique_ptr<NavigationItemImpl>
NavigationManagerImpl::CreateNavigationItemWithRewriters(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    NavigationInitiationType initiation_type,
    const GURL& previous_url,
    const std::vector<BrowserURLRewriter::URLRewriter>* additional_rewriters)
    const {
  GURL loaded_url(url);

  // Do not rewrite placeholder URL. Navigation code relies on this special URL
  // to implement native view and WebUI, and rewriter code should not be exposed
  // to this special type of about:blank URL.
  if (base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage) ||
      !IsPlaceholderUrl(url)) {
    bool url_was_rewritten = false;
    if (additional_rewriters && !additional_rewriters->empty()) {
      url_was_rewritten = web::BrowserURLRewriter::RewriteURLWithWriters(
          &loaded_url, browser_state_, *additional_rewriters);
    }

    if (!url_was_rewritten) {
      web::BrowserURLRewriter::GetInstance()->RewriteURLIfNecessary(
          &loaded_url, browser_state_);
    }
  }

  // The URL should not be changed to app-specific URL if the load is
  // renderer-initiated or a reload requested by non-app-specific URL. Pages
  // with app-specific urls have elevated previledges and should not be allowed
  // to open app-specific URLs.
  if ((initiation_type == web::NavigationInitiationType::RENDERER_INITIATED ||
       PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) &&
      loaded_url != url && web::GetWebClient()->IsAppSpecificURL(loaded_url) &&
      !web::GetWebClient()->IsAppSpecificURL(previous_url)) {
    loaded_url = url;
  }

  auto item = std::make_unique<NavigationItemImpl>();
  item->SetOriginalRequestURL(loaded_url);
  item->SetURL(loaded_url);
  item->SetReferrer(referrer);
  item->SetTransitionType(transition);
  item->SetNavigationInitiationType(initiation_type);

  return item;
}

NavigationItem* NavigationManagerImpl::GetLastCommittedItemWithUserAgentType()
    const {
  for (int index = GetLastCommittedItemIndexInCurrentOrRestoredSession();
       index >= 0; index--) {
    NavigationItem* item = GetItemAtIndex(index);
    if (wk_navigation_util::URLNeedsUserAgentType(item->GetURL())) {
      DCHECK_NE(item->GetUserAgentForInheritance(), UserAgentType::NONE);
      return item;
    }
  }
  return nullptr;
}

void NavigationManagerImpl::FinishReload() {
  delegate_->Reload();
}

void NavigationManagerImpl::FinishLoadURLWithParams(
    NavigationInitiationType initiation_type) {
  delegate_->LoadCurrentItem(initiation_type);
}

bool NavigationManagerImpl::IsPlaceholderUrl(const GURL& url) const {
  DCHECK(!base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage));
  return false;  // Default implementation doesn't use placeholder URLs
}

}  // namespace web
