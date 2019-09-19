// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/profiles/incognito_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#endif

namespace {

ProfileMenuViewBase* g_profile_bubble_ = nullptr;

// Helpers --------------------------------------------------------------------

constexpr int kMenuWidth = 288;
constexpr int kIconSize = 16;
constexpr int kIdentityImageSize = 64;
constexpr int kMaxImageSize = kIdentityImageSize;

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

// Spacing between the edge of the user menu and the top/bottom or left/right of
// the menu items.
constexpr int kMenuEdgeMargin = 16;

gfx::ImageSkia SizeImage(const gfx::ImageSkia& image, int size) {
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size(size, size));
}

gfx::ImageSkia ColorImage(const gfx::ImageSkia& image, SkColor color) {
  return gfx::ImageSkiaOperations::CreateColorMask(image, color);
}

std::unique_ptr<views::BoxLayout> CreateBoxLayout(
    views::BoxLayout::Orientation orientation,
    views::BoxLayout::CrossAxisAlignment cross_axis_alignment,
    gfx::Insets insets = gfx::Insets()) {
  auto layout = std::make_unique<views::BoxLayout>(orientation, insets);
  layout->set_cross_axis_alignment(cross_axis_alignment);
  return layout;
}

std::unique_ptr<views::View> CreateBorderedBoxView(
    std::unique_ptr<views::View> children_container) {
  constexpr int kBorderThickness = 1;
  const SkColor kBorderColor =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_MenuSeparatorColor);

  // Add rounded rectangular border around children.
  children_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  children_container->SetBorder(views::CreateRoundedRectBorder(
      kBorderThickness,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_HIGH),
      kBorderColor));

  // Create outer view with margin.
  // The outer view is needed because |BoxLayout| doesn't support outer
  // margins.
  auto outer_view = std::make_unique<views::View>();
  outer_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  constexpr auto kOuterMargin = gfx::Insets(16);
  outer_view->SetBorder(views::CreateEmptyBorder(kOuterMargin));
  outer_view->AddChildView(std::move(children_container));

  return outer_view;
}

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
    signin_metrics::AccessPoint access_point,
    views::Button* anchor_button,
    Browser* browser,
    bool is_source_keyboard) {
  if (IsShowing())
    return;

  base::RecordAction(base::UserMetricsAction("ProfileMenu_Opened"));

  ProfileMenuViewBase* bubble;
  if (view_mode == profiles::BUBBLE_VIEW_MODE_INCOGNITO) {
    DCHECK(browser->profile()->IsIncognitoProfile());
    bubble = new IncognitoMenuView(anchor_button, browser);
  } else {
    DCHECK_EQ(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER, view_mode);
#if !defined(OS_CHROMEOS)
    bubble = new ProfileMenuView(anchor_button, browser, access_point);
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
}

ProfileMenuViewBase::~ProfileMenuViewBase() {
  // Items stored for menu generation are removed after menu is finalized, hence
  // it's not expected to have while destroying the object.
  DCHECK(g_profile_bubble_ != this);
  DCHECK(menu_item_groups_.empty());
}

void ProfileMenuViewBase::SetIdentityInfo(const gfx::Image& image,
                                          const base::string16& title,
                                          const base::string16& subtitle) {
  constexpr int kTopMargin = 16;
  constexpr int kImageToLabelSpacing = 4;

  identity_info_container_->RemoveAllChildViews(/*delete_children=*/true);
  identity_info_container_->SetLayoutManager(
      CreateBoxLayout(views::BoxLayout::Orientation::kVertical,
                      views::BoxLayout::CrossAxisAlignment::kCenter));
  identity_info_container_->SetBorder(
      views::CreateEmptyBorder(kTopMargin, 0, 0, 0));

  views::ImageView* image_view = identity_info_container_->AddChildView(
      std::make_unique<views::ImageView>());
  // Fall back on |kUserAccountAvatarIcon| if |image| is empty. This can happen
  // in tests and when the account image hasn't been fetched yet.
  image_view->SetImage(
      image.IsEmpty()
          ? gfx::CreateVectorIcon(kUserAccountAvatarIcon, kIdentityImageSize,
                                  kIdentityImageSize)
          : profiles::GetSizedAvatarIcon(image, /*is_rectangle=*/true,
                                         kIdentityImageSize, kIdentityImageSize,
                                         profiles::SHAPE_CIRCLE)
                .AsImageSkia());

  views::View* title_label =
      identity_info_container_->AddChildView(std::make_unique<views::Label>(
          title, views::style::CONTEXT_DIALOG_TITLE));
  title_label->SetBorder(
      views::CreateEmptyBorder(kImageToLabelSpacing, 0, 0, 0));

  if (!subtitle.empty()) {
    identity_info_container_->AddChildView(std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  }
}

void ProfileMenuViewBase::SetSyncInfo(const base::string16& description,
                                      const base::string16& link_text,
                                      base::RepeatingClosure action) {
  constexpr int kVerticalPadding = 8;

  sync_info_container_->RemoveAllChildViews(/*delete_children=*/true);
  sync_info_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  views::View* separator =
      sync_info_container_->AddChildView(std::make_unique<views::Separator>());
  separator->SetBorder(
      views::CreateEmptyBorder(0, 0, /*bottom=*/kVerticalPadding, 0));

  if (!description.empty()) {
    views::Label* label = sync_info_container_->AddChildView(
        std::make_unique<views::Label>(description));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    label->SetHandlesTooltips(false);
    label->SetBorder(views::CreateEmptyBorder(gfx::Insets(0, kMenuEdgeMargin)));
  }

  views::Link* link = sync_info_container_->AddChildView(
      std::make_unique<views::Link>(link_text));
  link->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  link->set_listener(this);
  link->SetBorder(views::CreateEmptyBorder(/*top=*/0, /*left=*/kMenuEdgeMargin,
                                           /*bottom=*/kVerticalPadding,
                                           /*right=*/kMenuEdgeMargin));

  RegisterClickAction(link, std::move(action));
}

void ProfileMenuViewBase::AddShortcutFeatureButton(
    const gfx::VectorIcon& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  constexpr auto kMargins = gfx::Insets(8, 0);
  constexpr int kButtonSpacing = 6;
  constexpr int kIconSize = 16;
  constexpr int kIconPadding = 6;
  const SkColor kIconColor =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);
  const SkColor kBorderColor =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_MenuSeparatorColor);
  constexpr int kBorderThickness = 1;
  constexpr int kButtonRadius = kIconSize / 2 + kIconPadding;

  // Initialize layout if this is the first time a button is added.
  if (!shortcut_features_container_->GetLayoutManager()) {
    views::BoxLayout* layout = shortcut_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, kMargins,
            kButtonSpacing));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
  }

  // Add icon button with tooltip text(shown on hover).
  views::Button* button =
      shortcut_features_container_->AddChildView(std::make_unique<HoverButton>(
          this, gfx::CreateVectorIcon(icon, kIconSize, kIconColor),
          base::string16()));
  button->SetTooltipText(text);
  // Specify circular border with inside padding.
  auto circular_border = views::CreateRoundedRectBorder(
      kBorderThickness, kButtonRadius, kBorderColor);
  button->SetBorder(views::CreatePaddedBorder(std::move(circular_border),
                                              gfx::Insets(kIconPadding)));

  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::AddAccountFeatureButton(
    const gfx::ImageSkia& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  constexpr int kIconSize = 16;
  const SkColor kIconColor =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);

  // Initialize layout if this is the first time a button is added.
  if (!account_features_container_->GetLayoutManager()) {
    account_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
  }

  account_features_container_->AddChildView(
      std::make_unique<views::Separator>());

  views::Button* button =
      account_features_container_->AddChildView(std::make_unique<HoverButton>(
          this, SizeImage(ColorImage(icon, kIconColor), kIconSize), text));

  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::SetProfileHeading(const base::string16& heading) {
  profile_heading_container_->RemoveAllChildViews(/*delete_children=*/true);
  profile_heading_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());

  views::Label* label =
      profile_heading_container_->AddChildView(std::make_unique<views::Label>(
          heading, views::style::CONTEXT_LABEL, STYLE_HINT));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetHandlesTooltips(false);
  label->SetBorder(views::CreateEmptyBorder(gfx::Insets(0, kMenuEdgeMargin)));
}

void ProfileMenuViewBase::AddSelectableProfile(const gfx::Image& image,
                                               const base::string16& name,
                                               base::RepeatingClosure action) {
  constexpr auto kMargins = gfx::Insets(8, 0, 0, 0);
  constexpr int kImageSize = 22;

  // Initialize layout if this is the first time a button is added.
  if (!selectable_profiles_container_->GetLayoutManager()) {
    selectable_profiles_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical, kMargins));
  }

  gfx::Image sized_image =
      profiles::GetSizedAvatarIcon(image, /*is_rectangle=*/true, kImageSize,
                                   kImageSize, profiles::SHAPE_CIRCLE);
  views::Button* button = selectable_profiles_container_->AddChildView(
      std::make_unique<HoverButton>(this, sized_image.AsImageSkia(), name));

  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::AddProfileShortcutFeatureButton(
    const gfx::ImageSkia& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  constexpr int kIconSize = 28;

  // Initialize layout if this is the first time a button is added.
  if (!profile_shortcut_features_container_->GetLayoutManager()) {
    profile_shortcut_features_container_->SetLayoutManager(
        CreateBoxLayout(views::BoxLayout::Orientation::kHorizontal,
                        views::BoxLayout::CrossAxisAlignment::kCenter,
                        gfx::Insets(0, 0, 0, /*right=*/kMenuEdgeMargin)));
  }

  views::Button* button = profile_shortcut_features_container_->AddChildView(
      std::make_unique<HoverButton>(this, SizeImage(icon, kIconSize),
                                    base::string16()));
  button->SetTooltipText(text);
  button->SetBorder(nullptr);

  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::AddProfileFeatureButton(
    const gfx::ImageSkia& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  constexpr int kIconSize = 22;

  // Initialize layout if this is the first time a button is added.
  if (!profile_features_container_->GetLayoutManager()) {
    profile_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
  }

  views::Button* button = profile_features_container_->AddChildView(
      std::make_unique<HoverButton>(this, SizeImage(icon, kIconSize), text));

  RegisterClickAction(button, std::move(action));
}

gfx::ImageSkia ProfileMenuViewBase::ImageForMenu(const gfx::VectorIcon& icon,
                                                 float icon_to_image_ratio) {
  const SkColor kIconColor =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);
  const int padding =
      static_cast<int>(kMaxImageSize * (1.0 - icon_to_image_ratio) / 2.0);

  auto sized_icon =
      gfx::CreateVectorIcon(icon, kMaxImageSize - 2 * padding, kIconColor);
  return gfx::CanvasImageSource::CreatePadded(sized_icon, gfx::Insets(padding));
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

void ProfileMenuViewBase::Init() {
  Reset();
  BuildMenu();
  if (!base::FeatureList::IsEnabled(features::kProfileMenuRevamp))
    RepopulateViewFromMenuItems();
}

void ProfileMenuViewBase::WindowClosing() {
  DCHECK_EQ(g_profile_bubble_, this);
  if (anchor_button())
    anchor_button()->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  g_profile_bubble_ = nullptr;
}

void ProfileMenuViewBase::ButtonPressed(views::Button* button,
                                        const ui::Event& event) {
  OnClick(button);
}

void ProfileMenuViewBase::LinkClicked(views::Link* link, int event_flags) {
  OnClick(link);
}

void ProfileMenuViewBase::StyledLabelLinkClicked(views::StyledLabel* link,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  OnClick(link);
}

void ProfileMenuViewBase::OnClick(views::View* clickable_view) {
  DCHECK(!click_actions_[clickable_view].is_null());
  base::RecordAction(
      base::UserMetricsAction("ProfileMenu_ActionableItemClicked"));
  click_actions_[clickable_view].Run();
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
  if (!base::FeatureList::IsEnabled(features::kProfileMenuRevamp)) {
    menu_item_groups_.clear();
    return;
  }
  click_actions_.clear();
  RemoveAllChildViews(/*delete_childen=*/true);

  auto components = std::make_unique<views::View>();
  components->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Create and add new component containers in the correct order.
  // First, add the bordered box with the identity and feature buttons.
  auto bordered_box_container = std::make_unique<views::View>();
  identity_info_container_ =
      bordered_box_container->AddChildView(std::make_unique<views::View>());
  shortcut_features_container_ =
      bordered_box_container->AddChildView(std::make_unique<views::View>());
  sync_info_container_ =
      bordered_box_container->AddChildView(std::make_unique<views::View>());
  account_features_container_ =
      bordered_box_container->AddChildView(std::make_unique<views::View>());
  components->AddChildView(
      CreateBorderedBoxView(std::move(bordered_box_container)));
  // Second, add the profile header.
  auto profile_header = std::make_unique<views::View>();
  views::BoxLayout* profile_header_layout =
      profile_header->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  profile_heading_container_ =
      profile_header->AddChildView(std::make_unique<views::View>());
  profile_header_layout->SetFlexForView(profile_heading_container_, 1);
  profile_shortcut_features_container_ =
      profile_header->AddChildView(std::make_unique<views::View>());
  profile_header_layout->SetFlexForView(profile_shortcut_features_container_,
                                        0);
  components->AddChildView(std::move(profile_header));
  // Third, add the profile buttons at the bottom.
  selectable_profiles_container_ =
      components->AddChildView(std::make_unique<views::View>());
  profile_features_container_ =
      components->AddChildView(std::make_unique<views::View>());

  // Create a scroll view to hold the components.
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetHideHorizontalScrollBar(true);
  // TODO(https://crbug.com/871762): it's a workaround for the crash.
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());
  scroll_view->SetContents(std::move(components));

  // Create a grid layout to set the menu width.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                     kMenuWidth, kMenuWidth);
  layout->StartRow(1.0, 0);
  layout->AddView(std::move(scroll_view));
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
    base::RepeatingClosure action) {
  std::unique_ptr<HoverButton> title_card = std::make_unique<HoverButton>(
      this, std::move(icon_view), title, subtitle);
  if (action.is_null())
    title_card->SetEnabled(false);
  views::Button* button_ptr = title_card.get();
  RegisterClickAction(button_ptr, std::move(action));
  AddMenuItemInternal(std::move(title_card), MenuItems::kTitleCard);
  return button_ptr;
}

views::Button* ProfileMenuViewBase::CreateAndAddButton(
    const gfx::ImageSkia& icon,
    const base::string16& title,
    base::RepeatingClosure action) {
  std::unique_ptr<HoverButton> button =
      std::make_unique<HoverButton>(this, icon, title);
  views::Button* pointer = button.get();
  RegisterClickAction(pointer, std::move(action));
  AddMenuItemInternal(std::move(button), MenuItems::kButton);
  return pointer;
}

views::Button* ProfileMenuViewBase::CreateAndAddBlueButton(
    const base::string16& text,
    bool md_style,
    base::RepeatingClosure action) {
  std::unique_ptr<views::LabelButton> button =
      md_style ? views::MdTextButton::CreateSecondaryUiBlueButton(this, text)
               : views::MdTextButton::Create(this, text);
  views::Button* pointer = button.get();
  RegisterClickAction(pointer, std::move(action));

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
    gfx::Image* account_icon,
    base::RepeatingClosure action) {
  std::unique_ptr<DiceSigninButtonView> button =
      account_info ? std::make_unique<DiceSigninButtonView>(*account_info,
                                                            *account_icon, this)
                   : std::make_unique<DiceSigninButtonView>(this);
  DiceSigninButtonView* pointer = button.get();
  RegisterClickAction(pointer->signin_button(), std::move(action));

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
  label->SetMaximumWidth(kMenuWidth - 2 * kMenuEdgeMargin);
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

views::StyledLabel* ProfileMenuViewBase::CreateAndAddLabelWithLink(
    const base::string16& text,
    gfx::Range link_range,
    base::RepeatingClosure action) {
  auto label_with_link = std::make_unique<views::StyledLabel>(text, this);
  label_with_link->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  label_with_link->AddStyleRange(
      link_range, views::StyledLabel::RangeStyleInfo::CreateForLink());

  views::StyledLabel* pointer = label_with_link.get();
  RegisterClickAction(pointer, std::move(action));
  AddViewItem(std::move(label_with_link));
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

void ProfileMenuViewBase::RegisterClickAction(views::View* clickable_view,
                                              base::RepeatingClosure action) {
  DCHECK(click_actions_.count(clickable_view) == 0);
  click_actions_[clickable_view] = std::move(action);
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
                     kMenuWidth, kMenuWidth);
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
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor));
}

int ProfileMenuViewBase::GetDefaultIconSize() {
  return kIconSize;
}
