// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_ui_util.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "components/autofill/core/common/autofill_payments_features.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#endif

namespace autofill {

void UpdatePageActionIcon(PageActionIconType icon_type,
                          content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  // If feature is enabled, icon will be in the
  // ToolbarPageActionIconContainerView.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableToolbarStatusChip)) {
    PageActionIconContainer* toolbar_page_action_container =
        browser->window()->GetToolbarPageActionIconContainer();
    if (!toolbar_page_action_container)
      return;

    toolbar_page_action_container->UpdatePageActionIcon(icon_type);
  } else {
    // Otherwise the icon will be in the LocationBar.
    LocationBar* location_bar = browser->window()->GetLocationBar();
    if (!location_bar)
      return;

    switch (icon_type) {
      case PageActionIconType::kLocalCardMigration:
        location_bar->UpdateLocalCardMigrationIcon();
        break;
      case PageActionIconType::kSaveCard:
        location_bar->UpdateSaveCreditCardIcon();
        break;
      case PageActionIconType::kManagePasswords:
        browser->window()
            ->GetOmniboxPageActionIconContainer()
            ->UpdatePageActionIcon(icon_type);
        break;
      case PageActionIconType::kClickToCall:
      case PageActionIconType::kFind:
      case PageActionIconType::kIntentPicker:
      case PageActionIconType::kNativeFileSystemAccess:
      case PageActionIconType::kPwaInstall:
      case PageActionIconType::kReaderMode:
      case PageActionIconType::kSendTabToSelf:
      case PageActionIconType::kTranslate:
      case PageActionIconType::kZoom:
        NOTREACHED();
    }
  }
#endif
}

}  // namespace autofill
