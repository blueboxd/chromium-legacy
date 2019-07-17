// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
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

std::string GetBadgeString(base::Optional<uint64_t> badge_content) {
  if (!badge_content)
    return "•";

  if (badge_content > kMaxBadgeContent) {
    return base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
        IDS_SATURATED_BADGE_CONTENT, base::FormatNumber(kMaxBadgeContent)));
  }

  return base::UTF16ToUTF8(base::FormatNumber(badge_content.value()));
}

BadgeManager::BadgeManager(Profile* profile) {
#if defined(OS_MACOSX)
  SetDelegate(std::make_unique<BadgeManagerDelegateMac>(profile));
#elif defined(OS_WIN)
  SetDelegate(std::make_unique<BadgeManagerDelegateWin>(profile));
#endif
}

BadgeManager::~BadgeManager() = default;

void BadgeManager::SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate) {
  delegate_ = std::move(delegate);
}

void BadgeManager::BindRequest(blink::mojom::BadgeServiceRequest request,
                               content::RenderFrameHost* frame) {
  // TODO(crbug.com/983929): Remove these CHECKs once the cause of the bug has
  // been determined.
  CHECK(request);
  CHECK(frame);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  CHECK(web_contents);

  CHECK(web_contents->GetBrowserContext());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);

  badging::BadgeManager* badge_manager =
      badging::BadgeManagerFactory::GetInstance()->GetForProfile(profile);
  CHECK(badge_manager);

  CHECK(frame->GetProcess());
  CHECK(frame->GetProcess()->GetID());
  CHECK(frame->GetRoutingID());
  BindingContext context(frame->GetProcess()->GetID(), frame->GetRoutingID());

  badge_manager->bindings_.AddBinding(badge_manager, std::move(request),
                                      std::move(context));
}

void BadgeManager::UpdateAppBadge(const base::Optional<std::string>& app_id,
                                  base::Optional<uint64_t> content) {
  // Badge content should never be 0 (it should be translated into a clear).
  DCHECK_NE(content.value_or(1), 0u);

  if (!app_id) {
    BadgeChangeIgnored();
    return;
  }

  badged_apps_[app_id.value()] = content;

  if (!delegate_)
    return;

  delegate_->OnBadgeSet(app_id.value(), content);
}

void BadgeManager::ClearAppBadge(const base::Optional<std::string>& app_id) {
  if (!app_id) {
    BadgeChangeIgnored();
    return;
  }

  badged_apps_.erase(app_id.value());
  if (!delegate_)
    return;

  delegate_->OnBadgeCleared(app_id.value());
}

void BadgeManager::BadgeChangeIgnored() {
  if (!delegate_)
    return;

  delegate_->OnBadgeChangeIgnoredForTesting();
}

void BadgeManager::SetInteger(uint64_t content) {
  UpdateAppBadge(GetAppIdToBadge(bindings_.dispatch_context()), content);
}

void BadgeManager::SetFlag() {
  UpdateAppBadge(GetAppIdToBadge(bindings_.dispatch_context()), base::nullopt);
}

void BadgeManager::ClearBadge() {
  ClearAppBadge(GetAppIdToBadge(bindings_.dispatch_context()));
}

base::Optional<std::string> BadgeManager::GetAppIdToBadge(
    const BindingContext& context) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(context.process_id, context.frame_id);
  if (!frame)
    return base::nullopt;

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame);
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser)
    return base::nullopt;

  web_app::AppBrowserController* app_controller = browser->app_controller();
  if (!app_controller)
    return base::nullopt;

  // If the frame is not in scope, don't apply a badge.
  if (!app_controller->IsUrlInAppScope(frame->GetLastCommittedURL())) {
    return base::nullopt;
  }

  return app_controller->GetAppId();
}

}  // namespace badging
