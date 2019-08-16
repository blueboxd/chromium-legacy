// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credential_leak_dialog_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace {

// Creates the illustration which is rendered on top of the dialog.
std::unique_ptr<views::View> CreateIllustration() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<NonAccessibleImageView>();
  // TODO(crbug.com/986317): Replace with a proper image.
  image_view->SetImage(
      *rb.GetNativeImageNamed(IDR_PROFILES_DICE_TURN_ON_SYNC).ToImageSkia());
  return image_view;
}

// Creates the content containing the title and description for the dialog
// rendered below the illustration.
std::unique_ptr<views::View> CreateContent() {
  auto content = std::make_unique<views::View>();
  content->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical, gfx::Insets(),
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  content->SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::CONTROL, views::CONTROL)));

  auto title_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_TITLE),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content->AddChildView(std::move(title_label));

  auto description_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_CREDENTIAL_LEAK_MESSAGE),
      views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT);
  description_label->SetMultiLine(true);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content->AddChildView(std::move(description_label));

  return content;
}

}  // namespace

CredentialLeakDialogView::CredentialLeakDialogView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  DCHECK(controller);
  DCHECK(web_contents);
}

CredentialLeakDialogView::~CredentialLeakDialogView() = default;

void CredentialLeakDialogView::ShowCredentialLeakPrompt() {
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void CredentialLeakDialogView::ControllerGone() {
  // During Widget::Close() phase some accessibility event may occur. Thus,
  // |controller_| should be kept around.
  GetWidget()->Close();
  controller_ = nullptr;
}

ui::ModalType CredentialLeakDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

gfx::Size CredentialLeakDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

base::string16 CredentialLeakDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::DIALOG_BUTTON_OK ? IDS_CREDENTIAL_LEAK_OK : IDS_CLOSE);
}

void CredentialLeakDialogView::WindowClosing() {
  if (controller_) {
    controller_->OnCloseDialog();
  }
}

bool CredentialLeakDialogView::Accept() {
  if (controller_) {
    controller_->OnCheckPasswords();
  }
  return true;
}

void CredentialLeakDialogView::InitWindow() {
  SetLayoutManager(std::make_unique<BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      0 /* between_child_spacing */));
  std::unique_ptr<views::View> illustration = CreateIllustration();
  std::unique_ptr<views::View> content = CreateContent();
  AddChildView(std::move(illustration));
  AddChildView(std::move(content));
}

CredentialLeakPrompt* CreateCredentialLeakPromptView(
    CredentialLeakDialogController* controller,
    content::WebContents* web_contents) {
  return new CredentialLeakDialogView(controller, web_contents);
}
