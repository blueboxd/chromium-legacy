// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/reader_mode/reader_mode_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using dom_distiller::url_utils::IsDistilledPage;

ReaderModeIconView::ReaderModeIconView(CommandUpdater* command_updater,
                                       PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater, IDC_DISTILL_PAGE, delegate) {}

void ReaderModeIconView::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (GetVisible())
    AnimateInkDrop(views::InkDropState::HIDDEN, nullptr);
}

bool ReaderModeIconView::Update() {
  content::WebContents* contents = GetWebContents();
  if (!contents) {
    SetVisible(false);
    return false;
  }

  const bool was_previously_active = active();

  // TODO(gilmanmh): Display icon for only those pages that are likely to
  // render well in Reader Mode.
  SetVisible(true);
  SetActiveInternal(IsDistilledPage(contents->GetLastCommittedURL()));

  // Notify the icon when navigation to and from a distilled page occurs so that
  // it can hide the inkdrop.
  Observe(contents);

  return active() != was_previously_active;
}

const gfx::VectorIcon& ReaderModeIconView::GetVectorIcon() const {
  return kReaderModeIcon;
}

base::string16 ReaderModeIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(IDS_DISTILL_PAGE);
}

// TODO(gilmanmh): Consider displaying a bubble the first time a user
// activates the icon to explain what Reader Mode is.
views::BubbleDialogDelegateView* ReaderModeIconView::GetBubble() const {
  return nullptr;
}
