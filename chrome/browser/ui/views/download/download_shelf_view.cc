// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_shelf_view.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/download/download_item_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/download_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/mouse_watcher_view_host.h"

using download::DownloadItem;

namespace {

// TODO(pkasting): Replace these with LayoutProvider constants

// Padding above the content.
constexpr int kTopPadding = 1;

// Padding from left edge and first download view.
constexpr int kStartPadding = 4;

// Padding from right edge and close button/show downloads link.
constexpr int kEndPadding = 6;

// Padding between the show all link and close button.
constexpr int kCloseAndLinkPadding = 6;

}  // namespace

DownloadShelfView::DownloadShelfView(Browser* browser, BrowserView* parent)
    : DownloadShelf(browser, browser->profile()),
      AnimationDelegateViews(this),
      new_item_animation_(this),
      shelf_animation_(this),
      parent_(parent),
      mouse_watcher_(
          std::make_unique<views::MouseWatcherViewHost>(this, gfx::Insets()),
          this) {
  // Start out hidden: the shelf might be created but never shown in some
  // cases, like when installing a theme. See DownloadShelf::AddDownload().
  SetVisible(false);

  auto show_all_view = views::MdTextButton::Create(
      this, l10n_util::GetStringUTF16(IDS_SHOW_ALL_DOWNLOADS));
  show_all_view_ = AddChildView(std::move(show_all_view));

  auto close_button = views::CreateVectorImageButton(this);
  close_button->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button->SetFocusForPlatform();
  close_button_ = AddChildView(std::move(close_button));

  accessible_alert_ = AddChildView(std::make_unique<views::View>());

  if (gfx::Animation::ShouldRenderRichAnimation()) {
    new_item_animation_.SetSlideDuration(
        base::TimeDelta::FromMilliseconds(800));
    shelf_animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(120));
  } else {
    new_item_animation_.SetSlideDuration(base::TimeDelta());
    shelf_animation_.SetSlideDuration(base::TimeDelta());
  }

  GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_DOWNLOADS_BAR));
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kGroup);

  // Delay 5 seconds if the mouse leaves the shelf by way of entering another
  // window. This is much larger than the normal delay as opening a download is
  // most likely going to trigger a new window to appear over the button. Delay
  // a long time so that the user has a chance to quickly close the other app
  // and return to chrome with the download shelf still open.
  mouse_watcher_.set_notify_on_exit_time(base::TimeDelta::FromSeconds(5));
  SetID(VIEW_ID_DOWNLOAD_SHELF);
}

DownloadShelfView::~DownloadShelfView() = default;

bool DownloadShelfView::IsShowing() const {
  return GetVisible() && shelf_animation_.IsShowing();
}

bool DownloadShelfView::IsClosing() const {
  return shelf_animation_.IsClosing();
}

gfx::Size DownloadShelfView::CalculatePreferredSize() const {
  gfx::Size prefsize(kEndPadding + kStartPadding + kCloseAndLinkPadding, 0);

  // Enlarge the preferred size enough to hold various other views side-by-side.
  const auto adjust_size = [&prefsize](views::View* view) {
    const gfx::Size size = view->GetPreferredSize();
    prefsize.Enlarge(size.width(), 0);
    prefsize.set_height(std::max(size.height(), prefsize.height()));
  };
  adjust_size(close_button_);
  adjust_size(show_all_view_);
  // Add one download view to the preferred size.
  if (!download_views_.empty())
    adjust_size(download_views_.front());

  prefsize.Enlarge(0, kTopPadding);
  if (shelf_animation_.is_animating()) {
    prefsize.set_height(
        static_cast<int>(static_cast<double>(prefsize.height()) *
                         shelf_animation_.GetCurrentValue()));
  }
  return prefsize;
}

void DownloadShelfView::Layout() {
  // Let our base class layout our child views
  views::View::Layout();

  gfx::Size close_button_size = close_button_->GetPreferredSize();
  gfx::Size show_all_size = show_all_view_->GetPreferredSize();
  int max_download_x =
      std::max<int>(0, width() - kEndPadding - close_button_size.width() -
                           kCloseAndLinkPadding - show_all_size.width());
  // If there is not enough room to show the first download item, show the
  // "Show all downloads" link to the left to make it more visible that there is
  // something to see.
  bool show_link_only = !download_views_.empty() &&
                        (download_views_.back()->GetPreferredSize().width() >
                         (max_download_x - kStartPadding));
  int next_x = show_link_only ? kStartPadding : max_download_x;

  const auto center_y = [height = height()](int item_height) {
    return std::max((height - item_height) / 2, kTopPadding);
  };

  show_all_view_->SetBounds(next_x, center_y(show_all_size.height()),
                            show_all_size.width(), show_all_size.height());
  next_x += show_all_size.width() + kCloseAndLinkPadding;
  close_button_->SizeToPreferredSize();
  close_button_->SetPosition(
      gfx::Point(next_x, center_y(close_button_->height())));
  if (show_link_only) {
    // Let's hide all the items.
    for (auto ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri)
      (*ri)->SetVisible(false);
    return;
  }

  next_x = kStartPadding;
  for (auto ri = download_views_.rbegin(); ri != download_views_.rend(); ++ri) {
    gfx::Size view_size = (*ri)->GetPreferredSize();

    int x = next_x;

    // Figure out width of item.
    int item_width = view_size.width();
    if (new_item_animation_.is_animating() && ri == download_views_.rbegin()) {
      item_width = static_cast<int>(static_cast<double>(view_size.width()) *
                                    new_item_animation_.GetCurrentValue());
    }

    next_x += item_width;

    // Make sure our item can be contained within the shelf.
    if (next_x < max_download_x) {
      (*ri)->SetVisible(true);
      (*ri)->SetBounds(x, center_y(view_size.height()), item_width,
                       view_size.height());
    } else {
      (*ri)->SetVisible(false);
    }
  }
}

void DownloadShelfView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == &new_item_animation_) {
    Layout();
    SchedulePaint();
  } else if (animation == &shelf_animation_) {
    // Force a re-layout of the parent, which will call back into
    // GetPreferredSize, where we will do our animation. In the case where the
    // animation is hiding, we do a full resize - the fast resizing would
    // otherwise leave blank white areas where the shelf was and where the
    // user's eye is. Thankfully bottom-resizing is a lot faster than
    // top-resizing.
    parent_->ToolbarSizeChanged(shelf_animation_.IsShowing());
  }
}

void DownloadShelfView::AnimationEnded(const gfx::Animation* animation) {
  if (animation != &shelf_animation_)
    return;

  const bool shown = shelf_animation_.IsShowing();
  parent_->SetDownloadShelfVisible(shown);

  // If the shelf was explicitly closed by the user, there are further steps to
  // take to complete closing.
  if (shown || is_hidden())
    return;

  // When the close animation is complete, remove all completed downloads.
  size_t i = 0;
  while (i < download_views_.size()) {
    DownloadUIModel* download = download_views_[i]->model();
    DownloadItem::DownloadState state = download->GetState();
    bool is_transfer_done = state == DownloadItem::COMPLETE ||
                            state == DownloadItem::CANCELLED ||
                            state == DownloadItem::INTERRUPTED;
    if (is_transfer_done && !download->IsDangerous()) {
      RemoveDownloadView(download_views_[i]);
    } else {
      // Treat the item as opened when we close. This way if we get shown again
      // the user need not open this item for the shelf to auto-close.
      download->SetOpened(true);
      ++i;
    }
  }

  // If we had keyboard focus, calling SetVisible(false) causes keyboard focus
  // to be completely lost. To prevent this, we focus another view: the web
  // contents. TODO(collinbaker): https://crbug.com/846466 Fix
  // AccessiblePaneView::SetVisible or FocusManager to make this unnecessary.
  auto* focus_manager = GetFocusManager();
  if (focus_manager && Contains(focus_manager->GetFocusedView())) {
    parent_->contents_web_view()->RequestFocus();
  }

  SetVisible(false);
}

void DownloadShelfView::ButtonPressed(
    views::Button* button, const ui::Event& event) {
  if (button == close_button_)
    Close();
  else if (button == show_all_view_)
    chrome::ShowDownloads(browser());
  else
    NOTREACHED();
}

void DownloadShelfView::MouseMovedOutOfHost() {
  Close();
}

void DownloadShelfView::AutoClose() {
  if (std::all_of(download_views_.cbegin(), download_views_.cend(),
                  [](const auto* view) { return view->model()->GetOpened(); }))
    mouse_watcher_.Start(GetWidget()->GetNativeWindow());
}

void DownloadShelfView::RemoveDownloadView(View* view) {
  DCHECK(view);
  auto i = find(download_views_.begin(), download_views_.end(), view);
  DCHECK(i != download_views_.end());
  download_views_.erase(i);
  RemoveChildView(view);
  delete view;
  if (download_views_.empty())
    Close();
  else
    AutoClose();
  Layout();
  SchedulePaint();
}

void DownloadShelfView::ConfigureButtonForTheme(views::MdTextButton* button) {
  DCHECK(GetThemeProvider());

  button->SetEnabledTextColors(
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT));
  // For the normal theme, just use the default button bg color.
  base::Optional<SkColor> bg_color;
  if (!ThemeServiceFactory::GetForProfile(profile())->UsingDefaultTheme()) {
    // For custom themes, we have to make up a background color for the
    // button. Use a slight tint of the shelf background.
    bg_color = color_utils::BlendTowardMaxContrast(
        GetThemeProvider()->GetColor(ThemeProperties::COLOR_DOWNLOAD_SHELF),
        0x10);
  }
  button->SetBgColorOverride(bg_color);
}

void DownloadShelfView::DoShowDownload(
    DownloadUIModel::DownloadUIModelPtr download) {
  mouse_watcher_.Stop();

  const bool was_empty = download_views_.empty();

  // Insert the new view as the first child, so the logical child order matches
  // the visual order.  This ensures that tabbing through downloads happens in
  // the order users would expect.
  auto view = std::make_unique<DownloadItemView>(std::move(download), this,
                                                 accessible_alert_);
  download_views_.push_back(AddChildViewAt(std::move(view), 0));

  // Max number of download views we'll contain. Any time a view is added and
  // we already have this many download views, one is removed.
  // TODO(pkasting): Maybe this should use a min width instead.
  constexpr size_t kMaxDownloadViews = 15;
  if (download_views_.size() > kMaxDownloadViews)
    RemoveDownloadView(download_views_.front());

  new_item_animation_.Reset();
  new_item_animation_.Show();

  if (was_empty && !shelf_animation_.is_animating() && GetVisible()) {
    // Force a re-layout of the parent to adjust height of shelf properly.
    parent_->ToolbarSizeChanged(shelf_animation_.IsShowing());
  }
}

void DownloadShelfView::DoOpen() {
  SetVisible(true);
  shelf_animation_.Show();
}

void DownloadShelfView::DoClose() {
  parent_->SetDownloadShelfVisible(false);
  shelf_animation_.Hide();
}

void DownloadShelfView::DoHide() {
  SetVisible(false);
  parent_->ToolbarSizeChanged(false);
  parent_->SetDownloadShelfVisible(false);
}

void DownloadShelfView::DoUnhide() {
  SetVisible(true);
  parent_->ToolbarSizeChanged(true);
  parent_->SetDownloadShelfVisible(true);
}

void DownloadShelfView::OnPaintBorder(gfx::Canvas* canvas) {
  canvas->FillRect(gfx::Rect(0, 0, width(), 1),
                   GetThemeProvider()->GetColor(
                       ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR));
}

void DownloadShelfView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();

  ConfigureButtonForTheme(show_all_view_);

  SetBackground(views::CreateSolidBackground(
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_DOWNLOAD_SHELF)));

  views::SetImageFromVectorIcon(
      close_button_, vector_icons::kCloseRoundedIcon,
      GetThemeProvider()->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT));
}

views::View* DownloadShelfView::GetDefaultFocusableChild() {
  if (!download_views_.empty())
    return download_views_.back();

  return show_all_view_;
}
