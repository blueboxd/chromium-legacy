// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/activity_services/share_to_data_builder.h"

#include "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#include "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#include "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"
#include "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace activity_services {

ShareToData* ShareToDataForTab(Tab* tab, const GURL& shareURL) {
  DCHECK(tab);
  // For crash documented in crbug.com/503955, tab.url which is being passed
  // as a reference parameter caused a crash due to invalid address which
  // which suggests that |tab| may be deallocated along the way. Check that
  // tab is still valid by checking webState which would be deallocated if
  // tab is being closed.
  if (!tab.webState)
    return nil;

  BOOL is_original_title = NO;
  DCHECK(tab.webState->GetNavigationManager());
  web::NavigationItem* last_committed_item =
      tab.webState->GetNavigationManager()->GetLastCommittedItem();
  if (last_committed_item) {
    // Do not use WebState::GetTitle() as it returns the display title, not the
    // original page title.
    const base::string16& original_title = last_committed_item->GetTitle();
    if (!original_title.empty()) {
      // If the original page title exists, it is expected to match the Tab's
      // title. If this ever changes, then a decision has to be made on which
      // one should be used for sharing.
      DCHECK([tab_util::GetTabTitle(tab.webState)
          isEqual:base::SysUTF16ToNSString(original_title)]);
      is_original_title = YES;
    }
  }

  BOOL is_page_printable = [tab.webState->GetView() viewPrintFormatter] != nil;
  ThumbnailGeneratorBlock thumbnail_generator =
      activity_services::ThumbnailGeneratorForTab(tab);
  const GURL& finalURLToShare =
      !shareURL.is_empty() ? shareURL : tab.webState->GetVisibleURL();

  web::NavigationItem* visibleItem =
      tab.webState->GetNavigationManager()->GetVisibleItem();
  web::UserAgentType userAgent = web::UserAgentType::NONE;
  if (visibleItem)
    userAgent = visibleItem->GetUserAgentType();

  auto* helper = FindTabHelper::FromWebState(tab.webState);
  BOOL is_page_searchable =
      (helper && helper->CurrentPageSupportsFindInPage() &&
       !helper->IsFindUIActive());
  NSString* tab_title = tab_util::GetTabTitle(tab.webState);
  return [[ShareToData alloc] initWithShareURL:finalURLToShare
                                    visibleURL:tab.webState->GetVisibleURL()
                                         title:tab_title
                               isOriginalTitle:is_original_title
                               isPagePrintable:is_page_printable
                              isPageSearchable:is_page_searchable
                                     userAgent:userAgent
                            thumbnailGenerator:thumbnail_generator];
}

}  // namespace activity_services
