// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#endif

namespace {

ProfileMenuViewBase* g_profile_bubble_ = nullptr;

// Helpers --------------------------------------------------------------------

constexpr int kFixedMenuWidthPreDice = 240;
constexpr int kFixedMenuWidthDice = 288;
constexpr int kIconSize = 16;

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

// Spacing between the edge of the user menu and the top/bottom or left/right of
// the menu items.
constexpr int kMenuEdgeMargin = 16;

}  // namespace

// MenuItems--------------------------------------------------------------------

ProfileMenuViewBase::MenuItems::MenuItems()
    : first_item_type(ProfileMenuViewBase::MenuItems::kNone),
      last_item_type(ProfileMenuViewBase::MenuItems::kNone),
      different_item_types(false) {}

ProfileMenuViewBase::MenuItems::MenuItems(MenuItems&&) = default;
ProfileMenuViewBase::MenuItems::~MenuItems() = default;

// ProfileMenuViewBase ---------------------------------------------------------

// static
void ProfileMenuViewBase::ShowBubble(
    profiles::BubbleViewMode view_mode,
    const signin::ManageAccountsParams& manage_accounts_params,
    signin_metrics::AccessPoint access_point,
    views::Button* anchor_button,
    Browser* browser,
    bool is_source_keyboard) {
  if (IsShowing())
    return;

  DCHECK_EQ(browser->profile()->IsIncognitoProfile(),
            view_mode == profiles::BUBBLE_VIEW_MODE_INCOGNITO);

  ProfileMenuViewBase* bubble;
  if (view_mode == profiles::BUBBLE_VIEW_MODE_INCOGNITO) {
    bubble = new IncognitoMenuView(anchor_button, browser);
  } else {
#if !defined(OS_CHROMEOS)
    bubble = new ProfileChooserView(anchor_button, browser, view_mode,
                                    manage_accounts_params.service_type,
                                    access_point);
#else
    NOTREACHED();
    return;
#endif
  }

  views::BubbleDialogDelegateView::CreateBubble(bubble)->Show();
  if (is_source_keyboard)
    bubble->FocusButtonOnKeyboardOpen();
}

// static
bool ProfileMenuViewBase::IsShowing() {
  return g_profile_bubble_ != nullptr;
}

// static
void ProfileMenuViewBase::Hide() {
  if (g_profile_bubble_)
    g_profile_bubble_->GetWidget()->Close();
}

// static
ProfileMenuViewBase* ProfileMenuViewBase::GetBubbleForTesting() {
  return g_profile_bubble_;
}

ProfileMenuViewBase::ProfileMenuViewBase(views::Button* anchor_button,
                                         Browser* browser)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      menu_width_(0),
      anchor_button_(anchor_button),
      close_bubble_helper_(this, browser) {
  DCHECK(!g_profile_bubble_);
  g_profile_bubble_ = this;
  // TODO(sajadm): Remove when fixing https://crbug.com/822075
  // The sign in webview will be clipped on the bottom corners without these
  // margins, see related bug <http://crbug.com/593203>.
  set_margins(gfx::Insets(2, 0));
  DCHECK(anchor_button);
  anchor_button->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);

  EnableUpDownKeyboardAccelerators();
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kMenu);

  bool dice_enabled = AccountConsistencyModeManager::IsDiceEnabledForProfile(
      browser->profile());
  menu_width_ = dice_enabled ? kFixedMenuWidthDice : kFixedMenuWidthPreDice;
}

ProfileMenuViewBase::~ProfileMenuViewBase() {
  // Items stored for menu generation are removed after menu is finalized, hence
  // it's not expected to have while destroying the object.
  DCHECK(g_profile_bubble_ != this);
  DCHECK(menu_item_groups_.empty());
}

ax::mojom::Role ProfileMenuViewBase::GetAccessibleWindowRole() {
  // Return |ax::mojom::Role::kDialog| which will make screen readers announce
  // the following in the listed order:
  // the title of the dialog, labels (if any), the focused View within the
  // dialog (if any)
  return ax::mojom::Role::kDialog;
}

void ProfileMenuViewBase::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground)));
}

int ProfileMenuViewBase::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ProfileMenuViewBase::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void ProfileMenuViewBase::WindowClosing() {
  DCHECK_EQ(g_profile_bubble_, this);
  if (anchor_button())
    anchor_button()->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  g_profile_bubble_ = nullptr;
}

void ProfileMenuViewBase::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  chrome::ShowSettings(browser_);
}

int ProfileMenuViewBase::GetMaxHeight() const {
  gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect screen_space =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  int available_space = screen_space.bottom() - anchor_rect.bottom();
#if defined(OS_WIN)
  // On Windows the bubble can also be show to the top of the anchor.
  available_space =
      std::max(available_space, anchor_rect.y() - screen_space.y());
#endif
  return std::max(kMinimumScrollableContentHeight, available_space);
}

void ProfileMenuViewBase::Reset() {
  menu_item_groups_.clear();
}

int ProfileMenuViewBase::GetMarginSize(GroupMarginSize margin_size) const {
  switch (margin_size) {
    case kNone:
      return 0;
    case kTiny:
      return ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
    case kSmall:
      return ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
    case kLarge:
      return ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
  }
}

void ProfileMenuViewBase::AddMenuGroup(bool add_separator) {
  if (add_separator && !menu_item_groups_.empty()) {
    DCHECK(!menu_item_groups_.back().items.empty());
    menu_item_groups_.emplace_back();
  }

  menu_item_groups_.emplace_back();
}

void ProfileMenuViewBase::AddMenuItemInternal(std::unique_ptr<views::View> view,
                                              MenuItems::ItemType item_type) {
  DCHECK(!menu_item_groups_.empty());
  auto& current_group = menu_item_groups_.back();

  current_group.items.push_back(std::move(view));
  if (current_group.items.size() == 1) {
    current_group.first_item_type = item_type;
    current_group.last_item_type = item_type;
  } else {
    current_group.different_item_types |=
        current_group.last_item_type != item_type;
    current_group.last_item_type = item_type;
  }
}

views::Button* ProfileMenuViewBase::CreateAndAddTitleCard(
    std::unique_ptr<views::View> icon_view,
    const base::string16& title,
    const base::string16& subtitle,
    bool enabled) {
  std::unique_ptr<HoverButton> title_card = std::make_unique<HoverButton>(
      enabled ? this : nullptr, std::move(icon_view), title, subtitle);
  title_card->SetEnabled(enabled);
  views::Button* button_ptr = title_card.get();
  AddMenuItemInternal(std::move(title_card), MenuItems::kTitleCard);
  return button_ptr;
}

views::Button* ProfileMenuViewBase::CreateAndAddButton(
    const gfx::ImageSkia& icon,
    const base::string16& title) {
  std::unique_ptr<HoverButton> button =
      std::make_unique<HoverButton>(this, icon, title);
  views::Button* pointer = button.get();
  AddMenuItemInternal(std::move(button), MenuItems::kButton);
  return pointer;
}

views::Button* ProfileMenuViewBase::CreateAndAddBlueButton(
    const base::string16& text,
    bool md_style) {
  std::unique_ptr<views::LabelButton> button =
      md_style ? views::MdTextButton::CreateSecondaryUiBlueButton(this, text)
               : views::MdTextButton::Create(this, text);
  views::Button* pointer = button.get();

  // Add margins.
  std::unique_ptr<views::View> margined_view = std::make_unique<views::View>();
  margined_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kMenuEdgeMargin)));
  margined_view->AddChildView(std::move(button));

  AddMenuItemInternal(std::move(margined_view), MenuItems::kStyledButton);
  return pointer;
}

#if !defined(OS_CHROMEOS)
DiceSigninButtonView* ProfileMenuViewBase::CreateAndAddDiceSigninButton(
    AccountInfo* account_info,
    gfx::Image* account_icon) {
  std::unique_ptr<DiceSigninButtonView> button =
      account_info ? std::make_unique<DiceSigninButtonView>(
                         *account_info, *account_icon, this,
                         false /* show_drop_down_arrow */)
                   : std::make_unique<DiceSigninButtonView>(this);
  DiceSigninButtonView* pointer = button.get();

  // Add margins.
  std::unique_ptr<views::View> margined_view = std::make_unique<views::View>();
  margined_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(GetMarginSize(kSmall), kMenuEdgeMargin)));
  margined_view->AddChildView(std::move(button));

  AddMenuItemInternal(std::move(margined_view), MenuItems::kStyledButton);
  return pointer;
}
#endif

views::Label* ProfileMenuViewBase::CreateAndAddLabel(const base::string16& text,
                                                     int text_context) {
  std::unique_ptr<views::Label> label =
      std::make_unique<views::Label>(text, text_context);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMaximumWidth(menu_width_ - 2 * kMenuEdgeMargin);
  views::Label* pointer = label.get();

  // Add margins.
  std::unique_ptr<views::View> margined_view = std::make_unique<views::View>();
  margined_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kMenuEdgeMargin)));
  margined_view->AddChildView(std::move(label));

  AddMenuItemInternal(std::move(margined_view), MenuItems::kLabel);
  return pointer;
}

void ProfileMenuViewBase::AddViewItem(std::unique_ptr<views::View> view) {
  // Add margins.
  std::unique_ptr<views::View> margined_view = std::make_unique<views::View>();
  margined_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, kMenuEdgeMargin)));
  margined_view->AddChildView(std::move(view));
  AddMenuItemInternal(std::move(margined_view), MenuItems::kGeneral);
}

void ProfileMenuViewBase::RepopulateViewFromMenuItems() {
  RemoveAllChildViews(true);

  // Create a view to keep menu contents.
  auto contents_view = std::make_unique<views::View>();
  contents_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets()));

  for (unsigned group_index = 0; group_index < menu_item_groups_.size();
       group_index++) {
    MenuItems& group = menu_item_groups_[group_index];
    if (group.items.empty()) {
      // An empty group represents a separator.
      contents_view->AddChildView(new views::Separator());
    } else {
      views::View* sub_view = new views::View();
      GroupMarginSize top_margin;
      GroupMarginSize bottom_margin;
      GroupMarginSize child_spacing;

      if (group.first_item_type == MenuItems::kTitleCard ||
          group.first_item_type == MenuItems::kLabel) {
        top_margin = kTiny;
      } else {
        top_margin = kSmall;
      }

      if (group.last_item_type == MenuItems::kTitleCard) {
        bottom_margin = kTiny;
      } else if (group.last_item_type == MenuItems::kButton) {
        bottom_margin = kSmall;
      } else {
        bottom_margin = kLarge;
      }

      if (!group.different_item_types) {
        child_spacing = kNone;
      } else if (group.items.size() == 2 &&
                 group.first_item_type == MenuItems::kTitleCard &&
                 group.last_item_type == MenuItems::kButton) {
        child_spacing = kNone;
      } else {
        child_spacing = kLarge;
      }

      // Reduce margins if previous/next group is not a separator.
      if (group_index + 1 < menu_item_groups_.size() &&
          !menu_item_groups_[group_index + 1].items.empty()) {
        bottom_margin = kTiny;
      }
      if (group_index > 0 &&
          !menu_item_groups_[group_index - 1].items.empty()) {
        top_margin = kTiny;
      }

      sub_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets(GetMarginSize(top_margin), 0,
                      GetMarginSize(bottom_margin), 0),
          GetMarginSize(child_spacing)));

      for (std::unique_ptr<views::View>& item : group.items)
        sub_view->AddChildView(std::move(item));

      contents_view->AddChildView(sub_view);
    }
  }

  menu_item_groups_.clear();

  // Create a scroll view to hold contents view.
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHideHorizontalScrollBar(true);
  // TODO(https://crbug.com/871762): it's a workaround for the crash.
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());
  scroll_view->SetContents(std::move(contents_view));

  // Create a grid layout to set the menu width.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                     menu_width_, menu_width_);
  layout->StartRow(1.0, 0);
  layout->AddView(std::move(scroll_view));
  if (GetBubbleFrameView()) {
    SizeToContents();
    // SizeToContents() will perform a layout, but only if the size changed.
    Layout();
  }
}

gfx::ImageSkia ProfileMenuViewBase::CreateVectorIcon(
    const gfx::VectorIcon& icon) {
  return gfx::CreateVectorIcon(
      icon, kIconSize,
      ui::NativeTheme::GetInstanceForNativeUi()->SystemDarkModeEnabled()
          ? gfx::kGoogleGrey500
          : gfx::kChromeIconGrey);
}

int ProfileMenuViewBase::GetDefaultIconSize() {
  return kIconSize;
}

bool ProfileMenuViewBase::ShouldProvideInitiallyFocusedView() const {
#if defined(OS_MACOSX)
  // On Mac, buttons are not focusable when full keyboard access is turned off,
  // causing views::Widget to fall back to focusing the first focusable View.
  // This behavior is not desired in profile menus because of the menu-like
  // design using |HoverButtons|.
  if (!GetFocusManager() || !GetFocusManager()->keyboard_accessible())
    return false;
#endif
  return true;
}
