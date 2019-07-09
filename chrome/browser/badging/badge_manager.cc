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
  Profile* profile = Profile::FromBrowserContext(
      content::WebContents::FromRenderFrameHost(frame)->GetBrowserContext());

  badging::BadgeManager* badge_manager =
      badging::BadgeManagerFactory::GetInstance()->GetForProfile(profile);
  badge_manager->bindings_.AddBinding(
      badge_manager, std::move(request),
      {frame->GetProcess()->GetID(), frame->GetRoutingID()});
}

void BadgeManager::UpdateAppBadge(const std::string& app_id,
                                  base::Optional<uint64_t> content) {
  // Badge content should never be 0 (it should be translated into a clear).
  DCHECK_NE(content.value_or(1), 0u);

  badged_apps_[app_id] = content;

  if (!delegate_)
    return;

  delegate_->OnBadgeSet(app_id, content);
}

void BadgeManager::ClearAppBadge(const std::string& app_id) {
  badged_apps_.erase(app_id);
  if (!delegate_)
    return;

  delegate_->OnBadgeCleared(app_id);
}

void BadgeManager::SetInteger(uint64_t content) {
  auto app_id = GetAppIdToBadge(bindings_.dispatch_context());
  if (!app_id) {
    delegate_->OnBadgeChangeIgnoredForTesting();
    return;
  }

  UpdateAppBadge(app_id.value(), content);
}

void BadgeManager::SetFlag() {
  auto app_id = GetAppIdToBadge(bindings_.dispatch_context());
  if (!app_id) {
    delegate_->OnBadgeChangeIgnoredForTesting();
    return;
  }

  UpdateAppBadge(app_id.value(), base::nullopt);
}

void BadgeManager::ClearBadge() {
  auto app_id = GetAppIdToBadge(bindings_.dispatch_context());
  if (!app_id) {
    delegate_->OnBadgeChangeIgnoredForTesting();
    return;
  }

  ClearAppBadge(app_id.value());
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
