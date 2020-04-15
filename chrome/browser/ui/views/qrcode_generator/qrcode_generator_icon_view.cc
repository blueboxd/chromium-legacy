// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/base/l10n/l10n_util.h"

namespace qrcode_generator {

QRCodeGeneratorIconView::QRCodeGeneratorIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_QRCODE_GENERATOR,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_QRCODE_GENERATOR_ICON_LABEL));
}

QRCodeGeneratorIconView::~QRCodeGeneratorIconView() = default;

views::BubbleDialogDelegateView* QRCodeGeneratorIconView::GetBubble() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;

  QRCodeGeneratorBubbleController* bubble_controller =
      QRCodeGeneratorBubbleController::Get(web_contents);
  if (!bubble_controller)
    return nullptr;

  return static_cast<QRCodeGeneratorBubble*>(
      bubble_controller->qrcode_generator_bubble_view());
}

void QRCodeGeneratorIconView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  const OmniboxView* omnibox_view = delegate()->GetOmniboxView();
  if (!omnibox_view)
    return;

  // TODO(skare): Fix show and re-hide conditions.
  // Need this queried in more cases.
  if (omnibox_view->model()->has_focus() &&
      !omnibox_view->model()->user_input_in_progress()) {
    if (!GetVisible())
      SetVisible(true);
  }
}

void QRCodeGeneratorIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& QRCodeGeneratorIconView::GetVectorIcon() const {
  return kQrcodeGeneratorIcon;
}

const char* QRCodeGeneratorIconView::GetClassName() const {
  return "QRCodeGeneratorIconView";
}

base::string16 QRCodeGeneratorIconView::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_OMNIBOX_QRCODE_GENERATOR_ICON_TOOLTIP);
}

bool QRCodeGeneratorIconView::ShouldShowLabel() const {
  return false;
}

}  // namespace qrcode_generator
