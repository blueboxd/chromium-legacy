// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_MACOSX)
#include "chrome/browser/badging/badge_manager_delegate_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/badging/badge_manager_delegate_win.h"
#endif

namespace badging {

BadgeManager::BadgeManager(Profile* profile) {
#if defined(OS_MACOSX)
  SetDelegate(std::make_unique<BadgeManagerDelegateMac>(
      profile, this,
      &web_app::WebAppProviderBase::GetProviderBase(profile)->registrar()));
#elif defined(OS_WIN)
  SetDelegate(std::make_unique<BadgeManagerDelegateWin>(
      profile, this,
      &web_app::WebAppProviderBase::GetProviderBase(profile)->registrar()));
#endif
}

BadgeManager::~BadgeManager() = default;

void BadgeManager::SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate) {
  delegate_ = std::move(delegate);
}

void BadgeManager::BindReceiver(
    content::RenderFrameHost* frame,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  Profile* profile = Profile::FromBrowserContext(
      content::WebContents::FromRenderFrameHost(frame)->GetBrowserContext());

  badging::BadgeManager* badge_manager =
      badging::BadgeManagerFactory::GetInstance()->GetForProfile(profile);
  if (!badge_manager)
    return;

  BindingContext context(frame->GetProcess()->GetID(), frame->GetRoutingID());
  badge_manager->receivers_.Add(badge_manager, std::move(receiver),
                                std::move(context));
}

bool BadgeManager::HasMoreSpecificBadgeForUrl(const GURL& scope,
                                              const GURL& url) {
  return MostSpecificBadgeForScope(url).spec().size() > scope.spec().size();
}

base::Optional<BadgeManager::BadgeValue> BadgeManager::GetBadgeValue(
    const GURL& scope) {
  const GURL& most_specific = MostSpecificBadgeForScope(scope);
  if (most_specific == GURL::EmptyGURL())
    return base::nullopt;

  return base::make_optional(badged_scopes_[most_specific]);
}

void BadgeManager::SetBadgeForTesting(const GURL& scope, BadgeValue value) {
  UpdateBadge(scope, value);
}

void BadgeManager::ClearBadgeForTesting(const GURL& scope) {
  UpdateBadge(scope, base::nullopt);
}

void BadgeManager::UpdateBadge(const GURL& scope,
                               base::Optional<BadgeValue> value) {
  if (!value)
    badged_scopes_.erase(scope);
  else
    badged_scopes_[scope] = value.value();

  if (!delegate_)
    return;

  delegate_->OnBadgeUpdated(scope);
}

void BadgeManager::SetBadge(blink::mojom::BadgeValuePtr mojo_value) {
  if (mojo_value->is_number() && mojo_value->get_number() == 0) {
    mojo::ReportBadMessage(
        "|value| should not be zero when it is |number| (ClearBadge should be "
        "called instead)!");
    return;
  }

  const base::Optional<GURL> app_scope =
      GetAppScopeForContext(receivers_.current_context());
  if (!app_scope)
    return;

  // Convert the mojo badge representation into a BadgeManager::BadgeValue.
  BadgeValue value = mojo_value->is_flag()
                         ? base::nullopt
                         : base::make_optional(mojo_value->get_number());
  UpdateBadge(app_scope.value(), base::make_optional(value));
}

void BadgeManager::ClearBadge() {
  const base::Optional<GURL> app_scope =
      GetAppScopeForContext(receivers_.current_context());
  if (!app_scope)
    return;

  UpdateBadge(app_scope.value(), base::nullopt);
}

GURL BadgeManager::MostSpecificBadgeForScope(const GURL& scope) {
  const std::string& scope_string = scope.spec();
  GURL best_match = GURL::EmptyGURL();
  uint64_t longest_match = 0;

  for (const auto& pair : badged_scopes_) {
    const std::string& cur_scope_str = pair.first.spec();
    if (scope_string.find(cur_scope_str) != 0)
      continue;

    if (longest_match >= cur_scope_str.size())
      continue;

    longest_match = cur_scope_str.size();
    best_match = pair.first;
  }

  return best_match;
}

base::Optional<GURL> BadgeManager::GetAppScopeForContext(
    const BindingContext& context) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(context.process_id, context.frame_id);
  if (!frame)
    return base::nullopt;

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!contents)
    return base::nullopt;

  const web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(
          Profile::FromBrowserContext(contents->GetBrowserContext()))
          ->registrar();

  const base::Optional<web_app::AppId> app_id =
      registrar.FindAppWithUrlInScope(frame->GetLastCommittedURL());
  if (!app_id)
    return base::nullopt;

  return registrar.GetAppScope(app_id.value());
}

std::string GetBadgeString(base::Optional<uint64_t> badge_content) {
  if (!badge_content)
    return "•";

  if (badge_content > kMaxBadgeContent) {
    return base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
        IDS_SATURATED_BADGE_CONTENT, base::FormatNumber(kMaxBadgeContent)));
  }

  return base::UTF16ToUTF8(base::FormatNumber(badge_content.value()));
}

}  // namespace badging
