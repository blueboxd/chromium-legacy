// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// Icon sizes in DIP.
constexpr int kPrimaryIconSize = 20;
constexpr int kPrimaryIconBorderWidth = 8;
constexpr int kEmptyImageHeight = 88;
constexpr int kEmptyImageTopPadding = 16;

SkColor GetColorFromTheme() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  return native_theme->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
}

std::unique_ptr<views::ImageView> CreateIconView(const gfx::ImageSkia& icon) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(icon);
  icon_view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kPrimaryIconBorderWidth)));
  return icon_view;
}

std::unique_ptr<views::ImageView> CreateVectorIconView(
    const gfx::VectorIcon& vector_icon) {
  return CreateIconView(gfx::CreateVectorIcon(vector_icon, kPrimaryIconSize,
                                              GetColorFromTheme()));
}

std::unique_ptr<views::ImageView> CreateDeviceIcon(
    const sync_pb::SyncEnums::DeviceType device_type) {
  return CreateVectorIconView(device_type == sync_pb::SyncEnums::TYPE_TABLET
                                  ? kTabletIcon
                                  : kHardwareSmartphoneIcon);
}

std::unique_ptr<views::ImageView> CreateAppIcon(
    const ClickToCallUiController::App& app) {
  return app.vector_icon ? CreateVectorIconView(*app.vector_icon)
                         : CreateIconView(app.image.AsImageSkia());
}

std::unique_ptr<views::StyledLabel> CreateHelpText(
    views::StyledLabelListener* listener) {
  const base::string16 link = l10n_util::GetStringUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TROUBLESHOOT_LINK);
  size_t offset;
  const base::string16 text = l10n_util::GetStringFUTF16(
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES, link,
      &offset);
  auto label = std::make_unique<views::StyledLabel>(text, listener);
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  label->AddStyleRange(gfx::Range(offset, offset + link.length()), link_style);
  return label;
}

// TODO(himanshujaju): This is almost same as self share, we could unify these
// methods once we unify our architecture and dialog views.
base::string16 GetLastUpdatedTimeInDays(base::Time last_updated_timestamp) {
  int time_in_days = (base::Time::Now() - last_updated_timestamp).InDays();
  return l10n_util::GetPluralStringFUTF16(
      IDS_BROWSER_SHARING_DIALOG_DEVICE_SUBTITLE_LAST_ACTIVE_DAYS,
      time_in_days);
}

}  // namespace

ClickToCallDialogView::ClickToCallDialogView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    ClickToCallUiController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller),
      send_failed_(controller->HasSendFailed()) {}

ClickToCallDialogView::~ClickToCallDialogView() = default;

void ClickToCallDialogView::Hide() {
  CloseBubble();
}

int ClickToCallDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

std::unique_ptr<views::View> ClickToCallDialogView::CreateFootnoteView() {
  if (GetDialogType() != SharingDialogType::kDialogWithoutDevicesWithApp)
    return nullptr;

  return CreateHelpText(this);
}

void ClickToCallDialogView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                   const gfx::Range& range,
                                                   int event_flags) {
  controller_->OnHelpTextClicked();
}

SharingDialogType ClickToCallDialogView::GetDialogType() const {
  if (send_failed_)
    return SharingDialogType::kErrorDialog;

  bool has_devices = !controller_->devices().empty();
  bool has_apps = !controller_->apps().empty();

  if (has_devices)
    return SharingDialogType::kDialogWithDevicesMaybeApps;

  return has_apps ? SharingDialogType::kDialogWithoutDevicesWithApp
                  : SharingDialogType::kEducationalDialog;
}

void ClickToCallDialogView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  dialog_buttons_.clear();

  auto* provider = ChromeLayoutProvider::Get();
  SharingDialogType type = GetDialogType();
  LogClickToCallDialogShown(type);

  switch (type) {
    case SharingDialogType::kErrorDialog:
      set_margins(
          provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));
      InitErrorView();
      break;
    case SharingDialogType::kEducationalDialog:
      set_margins(
          provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));
      InitEmptyView();
      break;
    case SharingDialogType::kDialogWithoutDevicesWithApp:
    case SharingDialogType::kDialogWithDevicesMaybeApps:
      set_margins(
          gfx::Insets(provider->GetDistanceMetric(
                          views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL),
                      0,
                      provider->GetDistanceMetric(
                          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL),
                      0));
      InitListView();
      break;
  }

  // TODO(yasmo): See if GetWidget can be not null:
  if (GetWidget())
    SizeToContents();
}

void ClickToCallDialogView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (!sender || sender->tag() < 0)
    return;
  size_t index = static_cast<size_t>(sender->tag());
  const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices =
      controller_->devices();
  const std::vector<ClickToCallUiController::App>& apps = controller_->apps();
  DCHECK(index < devices.size() + apps.size());

  if (index < devices.size()) {
    LogClickToCallSelectedDeviceIndex(kSharingClickToCallUiDialog, index);
    controller_->OnDeviceChosen(*devices[index]);
    CloseBubble();
    return;
  }

  index -= devices.size();

  if (index < apps.size()) {
    LogClickToCallSelectedAppIndex(kSharingClickToCallUiDialog, index);
    controller_->OnAppChosen(apps[index]);
    CloseBubble();
  }
}

void ClickToCallDialogView::InitListView() {
  const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices =
      controller_->devices();
  const std::vector<ClickToCallUiController::App>& apps = controller_->apps();
  int tag = 0;

  // Devices:
  LogClickToCallDevicesToShow(kSharingClickToCallUiDialog, devices.size());
  for (const auto& device : devices) {
    auto dialog_button = std::make_unique<HoverButton>(
        this, CreateDeviceIcon(device->device_type()),
        base::UTF8ToUTF16(device->client_name()),
        GetLastUpdatedTimeInDays(device->last_updated_timestamp()));
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_buttons_.push_back(AddChildView(std::move(dialog_button)));
  }

  // Apps:
  LogClickToCallAppsToShow(kSharingClickToCallUiDialog, apps.size());
  for (const auto& app : apps) {
    auto dialog_button =
        std::make_unique<HoverButton>(this, CreateAppIcon(app), app.name,
                                      /* subtitle= */ base::string16());
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_buttons_.push_back(AddChildView(std::move(dialog_button)));
  }
}

void ClickToCallDialogView::InitEmptyView() {

  AddChildView(CreateHelpText(this));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  auto image_view = std::make_unique<NonAccessibleImageView>();
  const gfx::ImageSkia* image =
      rb.GetNativeImageNamed(IDR_SHARING_EMPTY_LIST).ToImageSkia();

  gfx::Size image_size(image->width(), image->height());
  const int image_width = image->width() * kEmptyImageHeight / image->height();
  image_size.SetToMin(gfx::Size(image_width, kEmptyImageHeight));
  image_view->SetImageSize(image_size);
  image_view->SetImage(*image);
  image_view->SetBorder(
      views::CreateEmptyBorder(kEmptyImageTopPadding, 0, 0, 0));

  AddChildView(std::move(image_view));
}

void ClickToCallDialogView::InitErrorView() {
  auto label = std::make_unique<views::Label>(controller_->GetErrorDialogText(),
                                              views::style::CONTEXT_LABEL,
                                              views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(label));
}

gfx::Size ClickToCallDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool ClickToCallDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 ClickToCallDialogView::GetWindowTitle() const {
  switch (GetDialogType()) {
    case SharingDialogType::kErrorDialog:
      return controller_->GetErrorDialogTitle();
    case SharingDialogType::kEducationalDialog:
      return l10n_util::GetStringUTF16(
          IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TITLE_NO_DEVICES);
    case SharingDialogType::kDialogWithoutDevicesWithApp:
    case SharingDialogType::kDialogWithDevicesMaybeApps:
      return controller_->GetTitle();
  }
}

void ClickToCallDialogView::WindowClosing() {
  controller_->OnDialogClosed(this);
}

// static
views::BubbleDialogDelegateView* ClickToCallDialogView::GetAsBubble(
    SharingDialog* dialog) {
  return static_cast<ClickToCallDialogView*>(dialog);
}
