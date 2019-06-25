// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_chooser_content_view.h"

#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr int kMessagePadding = 5;

// The lookup table for signal strength level image.
constexpr int kSignalStrengthLevelImageIds[5] = {
    IDR_SIGNAL_0_BAR, IDR_SIGNAL_1_BAR, IDR_SIGNAL_2_BAR, IDR_SIGNAL_3_BAR,
    IDR_SIGNAL_4_BAR};

constexpr int kHelpButtonTag = 1;
constexpr int kReScanButtonTag = 2;

}  // namespace

DeviceChooserContentView::BluetoothStatusContainer::BluetoothStatusContainer(
    views::ButtonListener* listener) {
  auto re_scan_button = views::MdTextButton::CreateSecondaryUiButton(
      listener,
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN));
  re_scan_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_RE_SCAN_TOOLTIP));
  re_scan_button->SetFocusForPlatform();
  re_scan_button->set_tag(kReScanButtonTag);
  re_scan_button_ = AddChildView(std::move(re_scan_button));

  throbber_ = AddChildView(std::make_unique<views::Throbber>());

  auto scanning_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_DISABLED);
  scanning_label->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL_TOOLTIP));
  scanning_label_ = AddChildView(std::move(scanning_label));
}

gfx::Size
DeviceChooserContentView::BluetoothStatusContainer::CalculatePreferredSize()
    const {
  const gfx::Size throbber_size = throbber_->GetPreferredSize();
  const gfx::Size scanning_label_size = scanning_label_->GetPreferredSize();
  const gfx::Size re_scan_button_size = re_scan_button_->GetPreferredSize();

  // The re-scan button and throbber plus label won't be shown at the same time,
  // so they overlap each other. The width is thus the larger of the two modes.
  const int width = std::max(throbber_size.width() + GetThrobberLabelSpacing() +
                                 scanning_label_size.width(),
                             re_scan_button_size.width());
  // The height is equal to the tallest of the child views.
  const int height = std::max(
      throbber_size.height(),
      std::max(scanning_label_size.height(), re_scan_button_size.height()));
  return gfx::Size(width, height);
}

void DeviceChooserContentView::BluetoothStatusContainer::Layout() {
  CenterVertically(re_scan_button_);
  CenterVertically(throbber_);
  CenterVertically(scanning_label_);
  scanning_label_->SetX(throbber_->bounds().right() +
                        GetThrobberLabelSpacing());
}

void DeviceChooserContentView::BluetoothStatusContainer::
    ShowScanningLabelAndThrobber() {
  re_scan_button_->SetVisible(false);
  throbber_->SetVisible(true);
  scanning_label_->SetVisible(true);
  throbber_->Start();
}

void DeviceChooserContentView::BluetoothStatusContainer::ShowReScanButton(
    bool enabled) {
  re_scan_button_->SetVisible(true);
  re_scan_button_->SetEnabled(enabled);
  throbber_->Stop();
  throbber_->SetVisible(false);
  scanning_label_->SetVisible(false);
}

int DeviceChooserContentView::BluetoothStatusContainer::
    GetThrobberLabelSpacing() const {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
}

void DeviceChooserContentView::BluetoothStatusContainer::CenterVertically(
    views::View* view) {
  view->SizeToPreferredSize();
  view->SetY((height() - view->height()) / 2);
}

DeviceChooserContentView::DeviceChooserContentView(
    views::TableViewObserver* table_view_observer,
    std::unique_ptr<ChooserController> chooser_controller)
    : chooser_controller_(std::move(chooser_controller)) {
  chooser_controller_->set_view(this);
  std::vector<ui::TableColumn> table_columns;
  table_columns.push_back(ui::TableColumn());
  auto table_view = std::make_unique<views::TableView>(
      this, table_columns,
      chooser_controller_->ShouldShowIconBeforeText() ? views::ICON_AND_TEXT
                                                      : views::TEXT_ONLY,
      !chooser_controller_->AllowMultipleSelection() /* single_selection */);
  table_view_ = table_view.get();
  table_view->set_select_on_remove(false);
  table_view->set_observer(table_view_observer);

  table_parent_ = AddChildView(
      views::TableView::CreateScrollViewWithTable(std::move(table_view)));

  no_options_help_ = new views::Label(chooser_controller_->GetNoOptionsText());
  no_options_help_->SetMultiLine(true);
  AddChildView(no_options_help_);

  base::string16 link_text = l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_TURN_ON_BLUETOOTH_LINK_TEXT);
  size_t offset = 0;
  base::string16 text = l10n_util::GetStringFUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_TURN_ADAPTER_OFF, link_text, &offset);
  adapter_off_help_ = new views::StyledLabel(text, this);
  adapter_off_help_->AddStyleRange(
      gfx::Range(0, link_text.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());
  adapter_off_help_->SetVisible(false);
  AddChildView(adapter_off_help_);

  UpdateTableView();
}

DeviceChooserContentView::~DeviceChooserContentView() {
  chooser_controller_->set_view(nullptr);
  table_view_->set_observer(nullptr);
  table_view_->SetModel(nullptr);
}

gfx::Size DeviceChooserContentView::GetMinimumSize() const {
  // Let the dialog shrink when its parent is smaller than the preferred size.
  return gfx::Size();
}

void DeviceChooserContentView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  table_parent_->SetBoundsRect(rect);

  // Place the "no devices found" and "adapter off" messages in the center of
  // the chooser.
  no_options_help_->SizeToFit(table_view_->width() - 2 * kMessagePadding);
  no_options_help_->SetPosition(
      gfx::Point((width() - no_options_help_->width()) / 2,
                 (height() - no_options_help_->height()) / 2));

  adapter_off_help_->SizeToFit(table_view_->width() - 2 * kMessagePadding);
  adapter_off_help_->SetPosition(
      gfx::Point((width() - adapter_off_help_->width()) / 2,
                 (height() - adapter_off_help_->height()) / 2));

  views::View::Layout();
}

gfx::Size DeviceChooserContentView::CalculatePreferredSize() const {
  return gfx::Size(402, 320);
}

int DeviceChooserContentView::RowCount() {
  return base::checked_cast<int>(chooser_controller_->NumOptions());
}

base::string16 DeviceChooserContentView::GetText(int row, int column_id) {
  int num_options = base::checked_cast<int>(chooser_controller_->NumOptions());
  DCHECK_GE(row, 0);
  DCHECK_LT(row, num_options);
  base::string16 text =
      chooser_controller_->GetOption(static_cast<size_t>(row));
  return chooser_controller_->IsPaired(row)
             ? l10n_util::GetStringFUTF16(
                   IDS_DEVICE_CHOOSER_DEVICE_NAME_AND_PAIRED_STATUS_TEXT, text)
             : text;
}

void DeviceChooserContentView::SetObserver(ui::TableModelObserver* observer) {}

gfx::ImageSkia DeviceChooserContentView::GetIcon(int row) {
  DCHECK(chooser_controller_->ShouldShowIconBeforeText());

  int num_options = base::checked_cast<int>(chooser_controller_->NumOptions());
  DCHECK_GE(row, 0);
  DCHECK_LT(row, num_options);

  if (chooser_controller_->IsConnected(row)) {
    return gfx::CreateVectorIcon(vector_icons::kBluetoothConnectedIcon,
                                 TableModel::kIconSize, gfx::kChromeIconGrey);
  }

  int level = chooser_controller_->GetSignalStrengthLevel(row);
  if (level == -1)
    return gfx::ImageSkia();

  DCHECK_GE(level, 0);
  DCHECK_LT(level, static_cast<int>(base::size(kSignalStrengthLevelImageIds)));

  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kSignalStrengthLevelImageIds[level]);
}

void DeviceChooserContentView::OnOptionsInitialized() {
  table_view_->OnModelChanged();
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionAdded(size_t index) {
  table_view_->OnItemsAdded(base::checked_cast<int>(index), 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionRemoved(size_t index) {
  table_view_->OnItemsRemoved(base::checked_cast<int>(index), 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnOptionUpdated(size_t index) {
  table_view_->OnItemsChanged(base::checked_cast<int>(index), 1);
  UpdateTableView();
}

void DeviceChooserContentView::OnAdapterEnabledChanged(bool enabled) {
  // No row is selected since the adapter status has changed.
  // This will also disable the OK button if it was enabled because
  // of a previously selected row.
  table_view_->Select(-1);
  adapter_enabled_ = enabled;
  UpdateTableView();

  bluetooth_status_container_->ShowReScanButton(enabled);

  if (GetWidget() && GetWidget()->GetRootView())
    GetWidget()->GetRootView()->Layout();
}

void DeviceChooserContentView::OnRefreshStateChanged(bool refreshing) {
  if (refreshing) {
    // No row is selected since the chooser is refreshing. This will also
    // disable the OK button if it was enabled because of a previously
    // selected row.
    table_view_->Select(-1);
    UpdateTableView();
  }

  if (refreshing)
    bluetooth_status_container_->ShowScanningLabelAndThrobber();
  else
    bluetooth_status_container_->ShowReScanButton(true /* enabled */);

  if (GetWidget() && GetWidget()->GetRootView())
    GetWidget()->GetRootView()->Layout();
}

void DeviceChooserContentView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  DCHECK_EQ(adapter_off_help_, label);
  chooser_controller_->OpenAdapterOffHelpUrl();
}

void DeviceChooserContentView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  if (sender->tag() == kHelpButtonTag) {
    chooser_controller_->OpenHelpCenterUrl();
  } else if (sender->tag() == kReScanButtonTag) {
    // Refreshing will cause the table view to yield focus, which
    // will land on the help button. Instead, briefly let the
    // rescan button take focus. When it hides itself, focus will
    // advance to the "Cancel" button as desired.
    sender->RequestFocus();
    chooser_controller_->RefreshOptions();
  } else {
    NOTREACHED();
  }
}

base::string16 DeviceChooserContentView::GetWindowTitle() const {
  return chooser_controller_->GetTitle();
}

std::unique_ptr<views::View> DeviceChooserContentView::CreateExtraView() {
  std::vector<std::unique_ptr<views::View>> extra_views;
  if (chooser_controller_->ShouldShowHelpButton()) {
    auto help_button = views::CreateVectorImageButton(this);
    views::SetImageFromVectorIcon(help_button.get(),
                                  vector_icons::kHelpOutlineIcon);
    help_button->SetFocusForPlatform();
    help_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
    help_button->set_tag(kHelpButtonTag);
    extra_views.push_back(std::move(help_button));
  }

  if (chooser_controller_->ShouldShowReScanButton()) {
    auto bluetooth_status_container =
        std::make_unique<BluetoothStatusContainer>(this);
    bluetooth_status_container_ = bluetooth_status_container.get();
    extra_views.push_back(std::move(bluetooth_status_container));
  }

  if (extra_views.empty())
    return nullptr;
  if (extra_views.size() == 1)
    return std::move(extra_views.at(0));

  auto container = std::make_unique<views::View>();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  container->SetLayoutManager(std::move(layout));

  for (auto& view : extra_views)
    container->AddChildView(std::move(view));
  return container;
}

base::string16 DeviceChooserContentView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK
             ? chooser_controller_->GetOkButtonLabel()
             : chooser_controller_->GetCancelButtonLabel();
}

bool DeviceChooserContentView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (chooser_controller_->BothButtonsAlwaysEnabled())
    return true;

  return button != ui::DIALOG_BUTTON_OK ||
         !table_view_->selection_model().empty();
}

void DeviceChooserContentView::Accept() {
  std::vector<size_t> indices(
      table_view_->selection_model().selected_indices().begin(),
      table_view_->selection_model().selected_indices().end());
  chooser_controller_->Select(indices);
}

void DeviceChooserContentView::Cancel() {
  chooser_controller_->Cancel();
}

void DeviceChooserContentView::Close() {
  chooser_controller_->Close();
}

void DeviceChooserContentView::UpdateTableView() {
  bool has_options = adapter_enabled_ && chooser_controller_->NumOptions() > 0;
  table_parent_->SetVisible(has_options);
  if (chooser_controller_->TableViewAlwaysDisabled())
    table_view_->SetEnabled(false);
  else
    table_view_->SetEnabled(has_options);
  no_options_help_->SetVisible(!has_options && adapter_enabled_);
  adapter_off_help_->SetVisible(!adapter_enabled_);
}
