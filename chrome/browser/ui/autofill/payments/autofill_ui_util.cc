// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_ui_util.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
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

  // If feature is disabled, kLocalCardMigration and kSaveCard will be children
  // of the LocationBarView.
  // TODO(crbug.com/788051): Make these always be children of a
  // |PageActionIconContainer| so we can just use |GetPageActionIcon|.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableToolbarStatusChip)) {
    LocationBar* location_bar = browser->window()->GetLocationBar();
    switch (icon_type) {
      case PageActionIconType::kLocalCardMigration:
        if (location_bar)
          location_bar->UpdateLocalCardMigrationIcon();
        return;
      case PageActionIconType::kSaveCard:
        if (location_bar)
          location_bar->UpdateSaveCreditCardIcon();
        return;
      case PageActionIconType::kManagePasswords: {
        // Handled by |UpdatePageActionIcon| below.
        break;
      }
      default:
        NOTREACHED();
    }
  }

  browser->window()->UpdatePageActionIcon(icon_type);
#endif
}

}  // namespace autofill
