// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_view.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// For bookmark in-product help.
int GetBookmarkPromoStringSpecifier() {
  static constexpr int kTextIds[] = {IDS_BOOKMARK_PROMO_0, IDS_BOOKMARK_PROMO_1,
                                     IDS_BOOKMARK_PROMO_2};
  const std::string& str = variations::GetVariationParamValue(
      "BookmarkInProductHelp", "x_promo_string");
  size_t text_specifier;
  if (!base::StringToSizeT(str, &text_specifier) ||
      text_specifier >= base::size(kTextIds)) {
    text_specifier = 0;
  }

  return kTextIds[text_specifier];
}

}  // namespace

StarView::StarView(CommandUpdater* command_updater,
                   Browser* browser,
                   PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater, IDC_BOOKMARK_PAGE, delegate),
      browser_(browser),
      bookmark_promo_observer_(this) {
  SetID(VIEW_ID_STAR_BUTTON);
  SetToggled(false);
}

StarView::~StarView() {}

void StarView::SetToggled(bool on) {
  PageActionIconView::SetActiveInternal(on);
}

void StarView::ShowPromo() {
  FeaturePromoBubbleView* bookmark_promo_bubble =
      FeaturePromoBubbleView::CreateOwned(
          this, views::BubbleBorder::TOP_RIGHT,
          FeaturePromoBubbleView::ActivationAction::ACTIVATE,
          GetBookmarkPromoStringSpecifier());
  if (!bookmark_promo_observer_.IsObserving(
          bookmark_promo_bubble->GetWidget())) {
    bookmark_promo_observer_.Add(bookmark_promo_bubble->GetWidget());
    SetActiveInternal(false);
    UpdateIconImage();
  }
}

void StarView::OnExecuting(PageActionIconView::ExecuteSource execute_source) {
  BookmarkEntryPoint entry_point = BOOKMARK_ENTRY_POINT_STAR_MOUSE;
  switch (execute_source) {
    case EXECUTE_SOURCE_MOUSE:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_MOUSE;
      break;
    case EXECUTE_SOURCE_KEYBOARD:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_KEY;
      break;
    case EXECUTE_SOURCE_GESTURE:
      entry_point = BOOKMARK_ENTRY_POINT_STAR_GESTURE;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.EntryPoint", entry_point,
                            BOOKMARK_ENTRY_POINT_LIMIT);
}

void StarView::ExecuteCommand(ExecuteSource source) {
  if (browser_) {
    OnExecuting(source);
    chrome::BookmarkCurrentPageIgnoringExtensionOverrides(browser_);
  } else {
    PageActionIconView::ExecuteCommand(source);
  }
}

views::BubbleDialogDelegateView* StarView::GetBubble() const {
  return BookmarkBubbleView::bookmark_bubble();
}

const gfx::VectorIcon& StarView::GetVectorIcon() const {
  return active() ? omnibox::kStarActiveIcon : omnibox::kStarIcon;
}

base::string16 StarView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(active() ? IDS_TOOLTIP_STARRED
                                            : IDS_TOOLTIP_STAR);
}

SkColor StarView::GetInkDropBaseColor() const {
  return bookmark_promo_observer_.IsObservingSources()
             ? GetNativeTheme()->GetSystemColor(
                   ui::NativeTheme::kColorId_ProminentButtonColor)
             : PageActionIconView::GetInkDropBaseColor();
}

void StarView::OnWidgetDestroying(views::Widget* widget) {
  if (bookmark_promo_observer_.IsObserving(widget)) {
    bookmark_promo_observer_.Remove(widget);
    SetActiveInternal(false);
    UpdateIconImage();
  }
}
