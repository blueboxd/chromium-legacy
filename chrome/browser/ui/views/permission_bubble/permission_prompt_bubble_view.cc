// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"

#include <memory>

#include "base/strings/string_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

PermissionPromptBubbleView::PermissionPromptBubbleView(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      name_or_origin_(GetDisplayNameOrOrigin()) {
  // Note that browser_ may be null in unit tests.
  DCHECK(delegate_);

  // To prevent permissions being accepted accidentally, and as a security
  // measure against crbug.com/619429, permission prompts should not be accepted
  // as the default action.
  DialogDelegate::SetDefaultButton(ui::DIALOG_BUTTON_NONE);

  DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  DialogDelegate::SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL, l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
  set_close_on_deactivate(false);

  DialogDelegate::SetAcceptCallback(
      base::BindOnce(&permissions::PermissionPrompt::Delegate::Accept,
                     base::Unretained(delegate)));
  DialogDelegate::SetCancelCallback(
      base::BindOnce(&permissions::PermissionPrompt::Delegate::Deny,
                     base::Unretained(delegate)));
  DialogDelegate::SetCloseCallback(
      base::BindOnce(&permissions::PermissionPrompt::Delegate::Closing,
                     base::Unretained(delegate)));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  for (permissions::PermissionRequest* request : delegate_->Requests())
    AddPermissionRequestLine(request);
}

void PermissionPromptBubbleView::Show() {
  DCHECK(browser_->window());

  // Set |parent_window| because some valid anchors can become hidden.
  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);
  // If a browser window (or popup) other than the bubble parent has focus,
  // don't take focus.
  if (browser_->window()->IsActive())
    widget->Show();
  else
    widget->ShowInactive();

  SizeToContents();
  UpdateAnchorPosition();
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PERMISSIONS);
}

void PermissionPromptBubbleView::AddPermissionRequestLine(
    permissions::PermissionRequest* request) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto* line_container = AddChildView(std::make_unique<views::View>());
  line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, provider->GetDistanceMetric(
                         DISTANCE_SUBSECTION_HORIZONTAL_INDENT)),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

  auto* icon =
      line_container->AddChildView(std::make_unique<views::ImageView>());
  const gfx::VectorIcon& vector_id = request->GetIconId();
  const SkColor icon_color = icon->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
  constexpr int kPermissionIconSize = 18;
  icon->SetImage(
      gfx::CreateVectorIcon(vector_id, kPermissionIconSize, icon_color));
  icon->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

  auto* label = line_container->AddChildView(
      std::make_unique<views::Label>(request->GetMessageTextFragment()));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);

  // Text that warns the user that flash will be deprecated. This text is only
  // shown for flash and should be empty for all other permissions.
  // TODO (crbug.com/1058401): Remove the warning text once flash is deprecated.
  const auto warning_text = request->GetMessageTextWarningFragment();

  if (warning_text.empty())
    return;

  // Should only be reached if the permission required is for flash.
  DCHECK(request->GetContentSettingsType() == ContentSettingsType::PLUGINS);

  auto* warning_line_container = AddChildView(std::make_unique<views::View>());
  warning_line_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, provider->GetDistanceMetric(
                         DISTANCE_SUBSECTION_HORIZONTAL_INDENT)),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));

  auto* warning_label = warning_line_container->AddChildView(
      std::make_unique<views::Label>(std::move(warning_text)));
  warning_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  warning_label->SetMultiLine(true);

  auto learn_more_button = views::CreateVectorImageButton(this);
  learn_more_button->SetFocusForPlatform();
  learn_more_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  SkColor text_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelEnabledColor);
  learn_more_button_ = learn_more_button.get();
  views::SetImageFromVectorIcon(learn_more_button_,
                                vector_icons::kHelpOutlineIcon, text_color);

  DialogDelegate::SetExtraView(std::move(learn_more_button));
}

void PermissionPromptBubbleView::UpdateAnchorPosition() {
  DCHECK(browser_->window());

  set_parent_window(
      platform_util::GetViewForWindow(browser_->window()->GetNativeWindow()));

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPageInfoAnchorConfiguration(browser_);
  SetAnchorView(configuration.anchor_view);
  SetHighlightedButton(configuration.highlighted_button);
  if (!configuration.anchor_view)
    SetAnchorRect(bubble_anchor_util::GetPageInfoAnchorRect(browser_));
  SetArrow(configuration.bubble_arrow);
}

void PermissionPromptBubbleView::AddedToWidget() {
  if (name_or_origin_.is_origin) {
    // There is a risk of URL spoofing from origins that are too wide to fit in
    // the bubble; elide origins from the front to prevent this.
    GetBubbleFrameView()->SetTitleView(
        CreateTitleOriginLabel(GetWindowTitle()));
  }
}

bool PermissionPromptBubbleView::ShouldShowCloseButton() const {
  return true;
}

base::string16 PermissionPromptBubbleView::GetWindowTitle() const {
  return l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                    name_or_origin_.name_or_origin);
}

base::string16 PermissionPromptBubbleView::GetAccessibleWindowTitle() const {
  // Generate one of:
  //   $origin wants to: $permission
  //   $origin wants to: $permission and $permission
  //   $origin wants to: $permission, $permission, and more
  // where $permission is the permission's text fragment, a verb phrase
  // describing what the permission is, like:
  //   "Download multiple files"
  //   "Use your camera"
  //
  // There are three separate internationalized messages used, one for each
  // format of title, to provide for accurate i18n. See https://crbug.com/434574
  // for more details.
  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();
  DCHECK(!requests.empty());

  if (requests.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_ONE_PERM,
        name_or_origin_.name_or_origin, requests[0]->GetMessageTextFragment());
  }

  int template_id =
      requests.size() == 2
          ? IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS
          : IDS_PERMISSIONS_BUBBLE_PROMPT_ACCESSIBLE_TITLE_TWO_PERMS_MORE;
  return l10n_util::GetStringFUTF16(template_id, name_or_origin_.name_or_origin,
                                    requests[0]->GetMessageTextFragment(),
                                    requests[1]->GetMessageTextFragment());
}

gfx::Size PermissionPromptBubbleView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void PermissionPromptBubbleView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender == learn_more_button_)
    chrome::AddSelectedTabWithURL(browser_,
                                  GURL(chrome::kFlashDeprecationLearnMoreURL),
                                  ui::PAGE_TRANSITION_LINK);
}

PermissionPromptBubbleView::DisplayNameOrOrigin
PermissionPromptBubbleView::GetDisplayNameOrOrigin() {
  const std::vector<permissions::PermissionRequest*>& requests =
      delegate_->Requests();
  DCHECK(!requests.empty());
  GURL origin_url = requests[0]->GetOrigin();

  if (origin_url.SchemeIs(extensions::kExtensionScheme)) {
    base::string16 extension_name =
        extensions::ui_util::GetEnabledExtensionNameForUrl(origin_url,
                                                           browser_->profile());
    if (!extension_name.empty())
      return {extension_name, false /* is_origin */};
  }

  // Web URLs should be displayed as the origin in the URL.
  return {url_formatter::FormatUrlForSecurityDisplay(
              origin_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          true /* is_origin */};
}
