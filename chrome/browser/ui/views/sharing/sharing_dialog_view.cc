// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
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
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#endif

namespace {

// Icon sizes in DIP.
constexpr int kPrimaryIconSize = 20;
constexpr int kPrimaryIconBorderWidth = 8;
constexpr int kHeaderImageHeight = 100;

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

gfx::ImageSkia CreateVectorIcon(const gfx::VectorIcon& vector_icon) {
  return gfx::CreateVectorIcon(vector_icon, kPrimaryIconSize,
                               GetColorFromTheme());
}

gfx::ImageSkia CreateDeviceIcon(
    const sync_pb::SyncEnums::DeviceType device_type) {
  return CreateVectorIcon(device_type == sync_pb::SyncEnums::TYPE_TABLET
                              ? kTabletIcon
                              : kHardwareSmartphoneIcon);
}

gfx::ImageSkia CreateAppIcon(const SharingUiController::App& app) {
  return app.vector_icon ? CreateVectorIcon(*app.vector_icon)
                         : app.image.AsImageSkia();
}

std::unique_ptr<views::StyledLabel> CreateHelpText(
    views::StyledLabelListener* listener) {
  const base::string16 link = l10n_util::GetStringUTF16(
      // TODO(yasmo): rename to IDS_BROWSER_SHARING_DIALOG_TROUBLESHOOT_LINK
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TROUBLESHOOT_LINK);
  size_t offset;
  const base::string16 text = l10n_util::GetStringFUTF16(
      // TODO(yasmo): rename to IDS_BROWSER_SHARING_DIALOG_HELP_TEXT_NO_DEVICES
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

SharingDialogView::SharingDialogView(views::View* anchor_view,
                                     content::WebContents* web_contents,
                                     SharingUiController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller),
      send_failed_(controller->HasSendFailed()) {}

SharingDialogView::~SharingDialogView() = default;

void SharingDialogView::Hide() {
  CloseBubble();
}

int SharingDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

std::unique_ptr<views::View> SharingDialogView::CreateFootnoteView() {
  if (GetDialogType() != SharingDialogType::kDialogWithoutDevicesWithApp)
    return nullptr;

  return CreateHelpText(this);
}

void SharingDialogView::StyledLabelLinkClicked(views::StyledLabel* label,
                                               const gfx::Range& range,
                                               int event_flags) {
  controller_->OnHelpTextClicked(GetDialogType());
}

SharingDialogType SharingDialogView::GetDialogType() const {
  if (send_failed_)
    return SharingDialogType::kErrorDialog;

  bool has_devices = !controller_->devices().empty();
  bool has_apps = !controller_->apps().empty();

  if (has_devices)
    return SharingDialogType::kDialogWithDevicesMaybeApps;

  return has_apps ? SharingDialogType::kDialogWithoutDevicesWithApp
                  : SharingDialogType::kEducationalDialog;
}

void SharingDialogView::Init() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  button_icons_.clear();

  auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets insets =
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT);

  SharingDialogType type = GetDialogType();
  LogSharingDialogShown(controller_->GetFeatureMetricsPrefix(), type);

  switch (type) {
    case SharingDialogType::kErrorDialog:
      InitErrorView();
      break;
    case SharingDialogType::kEducationalDialog:
      InitEmptyView();
      break;
    case SharingDialogType::kDialogWithoutDevicesWithApp:
    case SharingDialogType::kDialogWithDevicesMaybeApps:
      // Spread buttons across the whole dialog width.
      int top_margin = provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL);
      int bottom_margin = provider->GetDistanceMetric(
          views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL);
      insets = gfx::Insets(top_margin, 0, bottom_margin, 0);
      InitListView();
      break;
  }

  set_margins(gfx::Insets(insets.top(), 0, insets.bottom(), 0));
  SetBorder(views::CreateEmptyBorder(0, insets.left(), 0, insets.right()));

  // TODO(yasmo): See if GetWidget can be not null:
  if (GetWidget())
    SizeToContents();
}

void SharingDialogView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (!sender || sender->tag() < 0)
    return;
  size_t index = static_cast<size_t>(sender->tag());
  const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices =
      controller_->devices();
  const std::vector<SharingUiController::App>& apps = controller_->apps();
  DCHECK(index < devices.size() + apps.size());

  if (index < devices.size()) {
    LogSharingSelectedDeviceIndex(controller_->GetFeatureMetricsPrefix(),
                                  kSharingUiDialog, index);
    controller_->OnDeviceChosen(*devices[index]);
    CloseBubble();
    return;
  }

  index -= devices.size();

  if (index < apps.size()) {
    LogSharingSelectedAppIndex(controller_->GetFeatureMetricsPrefix(),
                               kSharingUiDialog, index);
    controller_->OnAppChosen(apps[index]);
    CloseBubble();
  }
}

void SharingDialogView::MaybeShowHeaderImage() {
  views::BubbleFrameView* frame_view = GetBubbleFrameView();
  if (!frame_view)
    return;

  int image_id = controller_->GetHeaderImageId();
  if (!image_id) {
    // Clear any previously set header image.
    frame_view->SetHeaderView(nullptr);
    return;
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* image = rb.GetNativeImageNamed(image_id).ToImageSkia();
  gfx::Size image_size(image->width(), image->height());
  const int image_width = image->width() * kHeaderImageHeight / image->height();
  image_size.SetToMin(gfx::Size(image_width, kHeaderImageHeight));

  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetImageSize(image_size);
  image_view->SetImage(*image);

  frame_view->SetHeaderView(std::move(image_view));
}

void SharingDialogView::AddedToWidget() {
  MaybeShowHeaderImage();
}

void SharingDialogView::OnThemeChanged() {
  LocationBarBubbleDelegateView::OnThemeChanged();
  MaybeShowHeaderImage();

  if (!button_icons_.size())
    return;

  const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices =
      controller_->devices();
  const std::vector<SharingUiController::App>& apps = controller_->apps();
  DCHECK_EQ(devices.size() + apps.size(), button_icons_.size());

  size_t button_index = 0;
  for (const auto& device : devices) {
    button_icons_[button_index]->SetImage(
        CreateDeviceIcon(device->device_type()));
    button_index++;
  }
  for (const auto& app : apps) {
    button_icons_[button_index]->SetImage(CreateAppIcon(app));
    button_index++;
  }
}

void SharingDialogView::InitListView() {
  const std::vector<std::unique_ptr<syncer::DeviceInfo>>& devices =
      controller_->devices();
  const std::vector<SharingUiController::App>& apps = controller_->apps();
  int tag = 0;

  // Devices:
  LogSharingDevicesToShow(controller_->GetFeatureMetricsPrefix(),
                          kSharingUiDialog, devices.size());
  for (const auto& device : devices) {
    auto icon = CreateIconView(CreateDeviceIcon(device->device_type()));
    button_icons_.push_back(icon.get());
    auto dialog_button = std::make_unique<HoverButton>(
        this, std::move(icon), base::UTF8ToUTF16(device->client_name()),
        GetLastUpdatedTimeInDays(device->last_updated_timestamp()));
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_buttons_.push_back(AddChildView(std::move(dialog_button)));
  }

  // Apps:
  LogSharingAppsToShow(controller_->GetFeatureMetricsPrefix(), kSharingUiDialog,
                       apps.size());
  for (const auto& app : apps) {
    auto icon = CreateIconView(CreateAppIcon(app));
    button_icons_.push_back(icon.get());
    auto dialog_button =
        std::make_unique<HoverButton>(this, std::move(icon), app.name,
                                      /* subtitle= */ base::string16());
    dialog_button->SetEnabled(true);
    dialog_button->set_tag(tag++);
    dialog_buttons_.push_back(AddChildView(std::move(dialog_button)));
  }
}

void SharingDialogView::InitEmptyView() {
  AddChildView(CreateHelpText(this));
}

void SharingDialogView::InitErrorView() {
  auto label = std::make_unique<views::Label>(controller_->GetErrorDialogText(),
                                              views::style::CONTEXT_LABEL,
                                              views::style::STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddChildView(std::move(label));
}

gfx::Size SharingDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_BUBBLE_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool SharingDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 SharingDialogView::GetWindowTitle() const {
  switch (GetDialogType()) {
    case SharingDialogType::kErrorDialog:
      return controller_->GetErrorDialogTitle();
    case SharingDialogType::kEducationalDialog:
      return controller_->GetEducationWindowTitleText();
    case SharingDialogType::kDialogWithoutDevicesWithApp:
    case SharingDialogType::kDialogWithDevicesMaybeApps:
      return controller_->GetTitle();
  }
}

void SharingDialogView::WindowClosing() {
  if (web_contents())
    controller_->OnDialogClosed(this);
}

// static
views::BubbleDialogDelegateView* SharingDialogView::GetAsBubble(
    SharingDialog* dialog) {
#if defined(OS_CHROMEOS)
  if (!dialog)
    return IntentPickerBubbleView::intent_picker_bubble();
#endif
  return static_cast<SharingDialogView*>(dialog);
}
