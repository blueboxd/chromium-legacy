// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

PwaInstallView::PwaInstallView(CommandUpdater* command_updater,
                               PageActionIconView::Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_PWA_INSTALL_ICON_LABEL));
  SetUpForInOutAnimation();
}

PwaInstallView::~PwaInstallView() {}

bool PwaInstallView::Update() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  auto* manager = banners::AppBannerManager::FromWebContents(web_contents);
  // May not be present e.g. in incognito mode.
  if (!manager)
    return false;

  bool is_probably_promotable = manager->IsProbablyPromotableWebApp();
  auto* tab_helper =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents);
  bool is_installed = tab_helper && tab_helper->HasAssociatedApp();

  bool show_install_button = is_probably_promotable && !is_installed;

  if (show_install_button && manager->MaybeConsumeInstallAnimation())
    AnimateIn(base::nullopt);
  else
    ResetSlideAnimation(false);

  bool was_visible = GetVisible();
  SetVisible(show_install_button);
  return GetVisible() != was_visible;
}

void PwaInstallView::OnExecuting(PageActionIconView::ExecuteSource source) {
  base::RecordAction(base::UserMetricsAction("PWAInstallIcon"));
  content::WebContents* web_contents = GetWebContents();
  // TODO(https://crbug.com/956810): Make AppBannerManager listen for
  // installations instead of having to notify it from every installation UI
  // surface.
  auto* manager = banners::AppBannerManager::FromWebContents(web_contents);
  web_app::CreateWebAppFromManifest(
      web_contents, WebappInstallSource::OMNIBOX_INSTALL_ICON,
      base::BindOnce(
          [](base::WeakPtr<banners::AppBannerManager> manager,
             const web_app::AppId& app_id, web_app::InstallResultCode code) {
            if (manager && code == web_app::InstallResultCode::kSuccess)
              manager->OnInstall(false, blink::kWebDisplayModeStandalone);
          },
          manager ? manager->GetWeakPtr() : nullptr));
}

views::BubbleDialogDelegateView* PwaInstallView::GetBubble() const {
  // TODO(https://907351): Implement.
  return nullptr;
}

const gfx::VectorIcon& PwaInstallView::GetVectorIcon() const {
  return omnibox::kPlusIcon;
}

base::string16 PwaInstallView::GetTextForTooltipAndAccessibleName() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return base::string16();
  return l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
      banners::AppBannerManager::GetInstallableWebAppName(web_contents));
}
