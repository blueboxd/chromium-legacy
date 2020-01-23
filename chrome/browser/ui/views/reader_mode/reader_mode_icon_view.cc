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

ReaderModeIconView::ReaderModeIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_DISTILL_PAGE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate) {}

ReaderModeIconView::~ReaderModeIconView() {
  content::WebContents* contents = web_contents();
  if (contents)
    dom_distiller::RemoveObserver(contents, this);
  DCHECK(!DistillabilityObserver::IsInObserverList());
}

void ReaderModeIconView::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (GetVisible())
    AnimateInkDrop(views::InkDropState::HIDDEN, nullptr);
}

void ReaderModeIconView::UpdateImpl() {
  content::WebContents* contents = GetWebContents();
  if (!contents) {
    SetVisible(false);
    return;
  }

  if (IsDistilledPage(contents->GetLastCommittedURL())) {
    SetVisible(true);
    SetActive(true);
  } else {
    // If the currently active web contents has changed since last time, stop
    // observing the old web contents and start observing the new one.
    // (WebContentsObserver::web_contents() is not updated until the call to
    // Observe() below, so it should still contain the old contents.)
    content::WebContents* old_contents = web_contents();
    if (old_contents != contents) {
      if (old_contents)
        dom_distiller::RemoveObserver(old_contents, this);
      dom_distiller::AddObserver(contents, this);
    }
    base::Optional<dom_distiller::DistillabilityResult> distillability =
        dom_distiller::GetLatestResult(contents);
    SetVisible(distillability && distillability.value().is_distillable);
    SetActive(false);
  }

  // Notify the icon when navigation to and from a distilled page occurs so that
  // it can hide the inkdrop.
  Observe(contents);
}

const gfx::VectorIcon& ReaderModeIconView::GetVectorIcon() const {
  return kReaderModeIcon;
}

base::string16 ReaderModeIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(IDS_DISTILL_PAGE);
}

const char* ReaderModeIconView::GetClassName() const {
  return "ReaderModeIconView";
}

// TODO(gilmanmh): Consider displaying a bubble the first time a user
// activates the icon to explain what Reader Mode is.
views::BubbleDialogDelegateView* ReaderModeIconView::GetBubble() const {
  return nullptr;
}

void ReaderModeIconView::OnResult(
    const dom_distiller::DistillabilityResult& result) {
  Update();
}
