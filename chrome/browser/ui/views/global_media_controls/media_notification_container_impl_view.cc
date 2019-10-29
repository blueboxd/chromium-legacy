// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"

#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// TODO(steimel): We need to decide on the correct values here.
constexpr int kWidth = 400;
constexpr gfx::Size kNormalSize = gfx::Size(kWidth, 100);
constexpr gfx::Size kExpandedSize = gfx::Size(kWidth, 150);
constexpr gfx::Size kDismissButtonSize = gfx::Size(30, 30);
constexpr int kDismissButtonIconSize = 20;
constexpr int kDismissButtonBackgroundRadius = 15;
constexpr SkColor kDefaultForegroundColor = SK_ColorBLACK;
constexpr SkColor kDefaultBackgroundColor = SK_ColorTRANSPARENT;

// The minimum number of enabled and visible user actions such that we should
// force the MediaNotificationView to be expanded.
constexpr int kMinVisibleActionsForExpanding = 4;

}  // anonymous namespace

class MediaNotificationContainerImplView::DismissButton
    : public views::ImageButton {
 public:
  explicit DismissButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
    views::ConfigureVectorImageButton(this);
  }

  ~DismissButton() override = default;

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), kDismissButtonBackgroundRadius);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DismissButton);
};

MediaNotificationContainerImplView::MediaNotificationContainerImplView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item)
    : views::Button(this),
      id_(id),
      foreground_color_(kDefaultForegroundColor),
      background_color_(kDefaultBackgroundColor) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetPreferredSize(kNormalSize);
  set_notify_enter_exit_on_child(true);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_BACK_TO_TAB));

  swipeable_container_ = std::make_unique<views::View>();
  swipeable_container_->set_owned_by_client();
  swipeable_container_->SetLayoutManager(std::make_unique<views::FillLayout>());
  swipeable_container_->SetPaintToLayer();
  swipeable_container_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(swipeable_container_.get());

  dismiss_button_placeholder_ = std::make_unique<views::View>();
  dismiss_button_placeholder_->set_owned_by_client();
  dismiss_button_placeholder_->SetPreferredSize(kDismissButtonSize);
  dismiss_button_placeholder_->SetLayoutManager(
      std::make_unique<views::FillLayout>());

  dismiss_button_container_ = std::make_unique<views::View>();
  dismiss_button_container_->set_owned_by_client();
  dismiss_button_container_->SetPreferredSize(kDismissButtonSize);
  dismiss_button_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  dismiss_button_container_->SetVisible(false);
  dismiss_button_placeholder_->AddChildView(dismiss_button_container_.get());

  auto dismiss_button = std::make_unique<DismissButton>(this);
  dismiss_button->SetPreferredSize(kDismissButtonSize);
  dismiss_button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  dismiss_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_GLOBAL_MEDIA_CONTROLS_DISMISS_ICON_TOOLTIP_TEXT));
  dismiss_button_ =
      dismiss_button_container_->AddChildView(std::move(dismiss_button));
  UpdateDismissButtonIcon();

  view_ = std::make_unique<media_message_center::MediaNotificationView>(
      this, std::move(item), dismiss_button_placeholder_.get(),
      base::string16(), kWidth, /*should_show_icon=*/false);
  view_->set_owned_by_client();
  ForceExpandedState();

  swipeable_container_->AddChildView(view_.get());

  slide_out_controller_ =
      std::make_unique<views::SlideOutController>(this, this);
}

MediaNotificationContainerImplView::~MediaNotificationContainerImplView() {
  for (auto& observer : observers_)
    observer.OnContainerDestroyed(id_);
}

void MediaNotificationContainerImplView::AddedToWidget() {
  if (GetFocusManager())
    GetFocusManager()->AddFocusChangeListener(this);
}

void MediaNotificationContainerImplView::RemovedFromWidget() {
  if (GetFocusManager())
    GetFocusManager()->RemoveFocusChangeListener(this);
}

void MediaNotificationContainerImplView::OnMouseEntered(
    const ui::MouseEvent& event) {
  UpdateDismissButtonVisibility();
}

void MediaNotificationContainerImplView::OnMouseExited(
    const ui::MouseEvent& event) {
  UpdateDismissButtonVisibility();
}

void MediaNotificationContainerImplView::OnDidChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  UpdateDismissButtonVisibility();
}

void MediaNotificationContainerImplView::OnExpanded(bool expanded) {
  SetPreferredSize(expanded ? kExpandedSize : kNormalSize);
  PreferredSizeChanged();

  for (auto& observer : observers_)
    observer.OnContainerExpanded(expanded);
}

void MediaNotificationContainerImplView::OnMediaSessionMetadataChanged() {
  for (auto& observer : observers_)
    observer.OnContainerMetadataChanged();
}

void MediaNotificationContainerImplView::OnVisibleActionsChanged(
    const base::flat_set<media_session::mojom::MediaSessionAction>& actions) {
  has_many_actions_ = actions.size() >= kMinVisibleActionsForExpanding;
  ForceExpandedState();
}

void MediaNotificationContainerImplView::OnMediaArtworkChanged(
    const gfx::ImageSkia& image) {
  has_artwork_ = !image.isNull();

  UpdateDismissButtonBackground();
  ForceExpandedState();
}

void MediaNotificationContainerImplView::OnColorsChanged(SkColor foreground,
                                                         SkColor background) {
  if (foreground_color_ != foreground) {
    foreground_color_ = foreground;
    UpdateDismissButtonIcon();
  }
  if (background_color_ != background) {
    background_color_ = background;
    UpdateDismissButtonBackground();
  }
}

ui::Layer* MediaNotificationContainerImplView::GetSlideOutLayer() {
  return swipeable_container_->layer();
}

void MediaNotificationContainerImplView::OnSlideOut() {
  DismissNotification();
}

void MediaNotificationContainerImplView::ButtonPressed(views::Button* sender,
                                                       const ui::Event& event) {
  if (sender == dismiss_button_) {
    DismissNotification();
  } else if (sender == this) {
    for (auto& observer : observers_)
      observer.OnContainerClicked(id_);
  } else {
    NOTREACHED();
  }
}

void MediaNotificationContainerImplView::AddObserver(
    MediaNotificationContainerObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationContainerImplView::RemoveObserver(
    MediaNotificationContainerObserver* observer) {
  observers_.RemoveObserver(observer);
}

views::ImageButton*
MediaNotificationContainerImplView::GetDismissButtonForTesting() {
  return dismiss_button_;
}

void MediaNotificationContainerImplView::UpdateDismissButtonIcon() {
  views::SetImageFromVectorIconWithColor(
      dismiss_button_, vector_icons::kCloseRoundedIcon, kDismissButtonIconSize,
      foreground_color_);
}

void MediaNotificationContainerImplView::UpdateDismissButtonBackground() {
  if (!has_artwork_) {
    dismiss_button_container_->SetBackground(nullptr);
    return;
  }

  dismiss_button_container_->SetBackground(views::CreateRoundedRectBackground(
      background_color_, kDismissButtonBackgroundRadius));
}

void MediaNotificationContainerImplView::UpdateDismissButtonVisibility() {
  bool has_focus = false;
  if (GetFocusManager()) {
    views::View* focused_view = GetFocusManager()->GetFocusedView();
    if (focused_view)
      has_focus = Contains(focused_view);
  }

  dismiss_button_container_->SetVisible(IsMouseHovered() || has_focus);
}

void MediaNotificationContainerImplView::DismissNotification() {
  for (auto& observer : observers_)
    observer.OnContainerDismissed(id_);
}

void MediaNotificationContainerImplView::ForceExpandedState() {
  if (view_) {
    bool should_expand = has_many_actions_ || has_artwork_;
    view_->SetForcedExpandedState(&should_expand);
  }
}
