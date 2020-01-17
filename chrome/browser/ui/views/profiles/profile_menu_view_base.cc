// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
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
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/browser/ui/views/sync/dice_signin_button_view.h"
#endif

namespace {

ProfileMenuViewBase* g_profile_bubble_ = nullptr;

// Helpers --------------------------------------------------------------------

constexpr int kMenuWidth = 288;
constexpr int kIdentityImageSize = 64;
constexpr int kMaxImageSize = kIdentityImageSize;
constexpr int kDefaultVerticalMargin = 8;

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

// Spacing between the edge of the user menu and the top/bottom or left/right of
// the menu items.
constexpr int kMenuEdgeMargin = 16;

SkColor GetDefaultIconColor() {
  return ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
}

SkColor GetDefaultSeparatorColor() {
  return ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
      ui::NativeTheme::kColorId_MenuSeparatorColor);
}

gfx::ImageSkia SizeImage(const gfx::ImageSkia& image, int size) {
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size(size, size));
}

gfx::ImageSkia ColorImage(const gfx::ImageSkia& image, SkColor color) {
  return gfx::ImageSkiaOperations::CreateColorMask(image, color);
}

class CircleImageSource : public gfx::CanvasImageSource {
 public:
  CircleImageSource(int size, SkColor color)
      : gfx::CanvasImageSource(gfx::Size(size, size)), color_(color) {}
  ~CircleImageSource() override = default;

  void Draw(gfx::Canvas* canvas) override;

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(CircleImageSource);
};

void CircleImageSource::Draw(gfx::Canvas* canvas) {
  float radius = size().width() / 2.0f;
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color_);
  canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
}

gfx::ImageSkia CreateCircle(int size, SkColor color = SK_ColorWHITE) {
  return gfx::CanvasImageSource::MakeImageSkia<CircleImageSource>(size, color);
}

gfx::ImageSkia CropCircle(const gfx::ImageSkia& image) {
  DCHECK_EQ(image.width(), image.height());
  return gfx::ImageSkiaOperations::CreateMaskedImage(
      image, CreateCircle(image.width()));
}

gfx::ImageSkia AddCircularBackground(const gfx::ImageSkia& image,
                                     SkColor bg_color,
                                     int size) {
  if (image.isNull())
    return gfx::ImageSkia();

  return gfx::ImageSkiaOperations::CreateSuperimposedImage(
      CreateCircle(size, bg_color), image);
}

std::unique_ptr<views::BoxLayout> CreateBoxLayout(
    views::BoxLayout::Orientation orientation,
    views::BoxLayout::CrossAxisAlignment cross_axis_alignment,
    gfx::Insets insets = gfx::Insets()) {
  auto layout = std::make_unique<views::BoxLayout>(orientation, insets);
  layout->set_cross_axis_alignment(cross_axis_alignment);
  return layout;
}

std::unique_ptr<views::Button> CreateCircularImageButton(
    views::ButtonListener* listener,
    const gfx::ImageSkia& image,
    const base::string16& text,
    bool show_border = false) {
  constexpr int kImageSize = 28;
  const int kBorderThickness = show_border ? 1 : 0;
  const SkScalar kButtonRadius = (kImageSize + 2 * kBorderThickness) / 2.0;

  auto button = std::make_unique<views::ImageButton>(listener);
  button->SetImage(views::Button::STATE_NORMAL, SizeImage(image, kImageSize));
  button->SetTooltipText(text);
  button->SetInkDropMode(views::Button::InkDropMode::ON);
  button->SetFocusForPlatform();
  button->set_ink_drop_base_color(GetDefaultIconColor());
  if (show_border) {
    button->SetBorder(views::CreateRoundedRectBorder(
        kBorderThickness, kButtonRadius, GetDefaultSeparatorColor()));
  }

  InstallCircleHighlightPathGenerator(button.get());

  return button;
}

}  // namespace

// ProfileMenuViewBase ---------------------------------------------------------

// static
void ProfileMenuViewBase::ShowBubble(
    profiles::BubbleViewMode view_mode,
    views::Button* anchor_button,
    Browser* browser,
    bool is_source_keyboard) {
  if (IsShowing())
    return;

  signin_ui_util::RecordProfileMenuViewShown(browser->profile());

  ProfileMenuViewBase* bubble;

  if (view_mode == profiles::BUBBLE_VIEW_MODE_INCOGNITO) {
    DCHECK(browser->profile()->IsIncognitoProfile());
    bubble = new IncognitoMenuView(anchor_button, browser);
  } else {
    DCHECK_EQ(profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER, view_mode);
#if !defined(OS_CHROMEOS)
    bubble = new ProfileMenuView(anchor_button, browser);
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
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
  // TODO(tluk): Remove when fixing https://crbug.com/822075
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
}

void ProfileMenuViewBase::SetHeading(const base::string16& heading,
                                     const base::string16& tooltip_text,
                                     base::RepeatingClosure action) {
  constexpr int kInsidePadding = 8;
  const SkColor kBackgroundColor =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_HighlightedMenuItemBackgroundColor);

  heading_container_->RemoveAllChildViews(/*delete_children=*/true);
  heading_container_->SetLayoutManager(std::make_unique<views::FillLayout>());
  heading_container_->SetBackground(
      views::CreateSolidBackground(kBackgroundColor));

  views::LabelButton* button = heading_container_->AddChildView(
      std::make_unique<HoverButton>(this, heading));
  button->SetEnabledTextColors(views::style::GetColor(
      *this, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  button->SetTooltipText(tooltip_text);
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets(kInsidePadding)));
  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::SetIdentityInfo(const gfx::ImageSkia& image,
                                          const gfx::ImageSkia& badge,
                                          const base::string16& title,
                                          const base::string16& subtitle) {
  constexpr int kTopMargin = kMenuEdgeMargin;
  constexpr int kBottomMargin = kDefaultVerticalMargin;
  constexpr int kHorizontalMargin = kMenuEdgeMargin;
  constexpr int kImageBottomMargin = kDefaultVerticalMargin;
  constexpr int kBadgeSize = 16;
  constexpr int kBadgePadding = 1;
  const SkColor kBadgeBackgroundColor = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_BubbleBackground);

  identity_info_container_->RemoveAllChildViews(/*delete_children=*/true);
  identity_info_container_->SetLayoutManager(
      CreateBoxLayout(views::BoxLayout::Orientation::kVertical,
                      views::BoxLayout::CrossAxisAlignment::kCenter,
                      gfx::Insets(kTopMargin, kHorizontalMargin, kBottomMargin,
                                  kHorizontalMargin)));

  views::ImageView* image_view = identity_info_container_->AddChildView(
      std::make_unique<views::ImageView>());
  // Fall back on |kUserAccountAvatarIcon| if |image| is empty. This can happen
  // in tests and when the account image hasn't been fetched yet.
  gfx::ImageSkia sized_image =
      image.isNull()
          ? gfx::CreateVectorIcon(kUserAccountAvatarIcon, kIdentityImageSize,
                                  GetDefaultIconColor())
          : CropCircle(SizeImage(image, kIdentityImageSize));
  gfx::ImageSkia sized_badge =
      AddCircularBackground(SizeImage(badge, kBadgeSize), kBadgeBackgroundColor,
                            kBadgeSize + 2 * kBadgePadding);
  gfx::ImageSkia sized_badge_with_shadow =
      gfx::ImageSkiaOperations::CreateImageWithDropShadow(
          sized_badge,
          gfx::ShadowValue::MakeMdShadowValues(/*elevation=*/1, SK_ColorBLACK));

  gfx::ImageSkia badged_image = gfx::ImageSkiaOperations::CreateIconWithBadge(
      sized_image, sized_badge_with_shadow);
  image_view->SetImage(badged_image);
  image_view->SetBorder(views::CreateEmptyBorder(0, 0, kImageBottomMargin, 0));

  if (!title.empty()) {
    identity_info_container_->AddChildView(std::make_unique<views::Label>(
        title, views::style::CONTEXT_DIALOG_TITLE));
  }

  if (!subtitle.empty()) {
    identity_info_container_->AddChildView(std::make_unique<views::Label>(
        subtitle, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  }
}

void ProfileMenuViewBase::SetSyncInfo(const gfx::ImageSkia& icon,
                                      const base::string16& description,
                                      const base::string16& clickable_text,
                                      base::RepeatingClosure action) {
  constexpr int kIconSize = 16;
  const int kDescriptionIconSpacing =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  constexpr int kInsidePadding = 12;
  constexpr int kBorderThickness = 1;
  const int kBorderCornerRadius =
      views::LayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_HIGH);

  sync_info_container_->RemoveAllChildViews(/*delete_children=*/true);
  sync_info_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kInsidePadding));

  if (description.empty()) {
    views::Button* button =
        sync_info_container_->AddChildView(std::make_unique<HoverButton>(
            this, SizeImage(icon, kIconSize), clickable_text));
    RegisterClickAction(button, std::move(action));
    return;
  }

  // Add padding, rounded border and margins.
  sync_info_container_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(kBorderThickness, kBorderCornerRadius,
                                     GetDefaultSeparatorColor()),
      gfx::Insets(kInsidePadding)));
  sync_info_container_->SetProperty(
      views::kMarginsKey, gfx::Insets(kDefaultVerticalMargin, kMenuEdgeMargin));

  // Add icon + description at the top.
  views::View* description_container =
      sync_info_container_->AddChildView(std::make_unique<views::View>());
  views::BoxLayout* description_layout =
      description_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
              kDescriptionIconSpacing));

  if (icon.isNull()) {
    // If there is no image, the description is centered.
    description_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
  } else {
    views::ImageView* icon_view = description_container->AddChildView(
        std::make_unique<views::ImageView>());
    icon_view->SetImage(SizeImage(icon, kIconSize));
  }

  views::Label* label = description_container->AddChildView(
      std::make_unique<views::Label>(description));
  label->SetMultiLine(true);
  label->SetHandlesTooltips(false);

  // Add blue button at the bottom.
  views::Button* button = sync_info_container_->AddChildView(
      views::MdTextButton::CreateSecondaryUiBlueButton(this, clickable_text));
  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::SetSyncInfoBackgroundColor(SkColor bg_color) {
  sync_info_container_->SetBackground(views::CreateRoundedRectBackground(
      bg_color, views::LayoutProvider::Get()->GetCornerRadiusMetric(
                    views::EMPHASIS_HIGH)));
}

void ProfileMenuViewBase::AddShortcutFeatureButton(
    const gfx::ImageSkia& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  const int kButtonSpacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_BUTTON_HORIZONTAL);

  // Initialize layout if this is the first time a button is added.
  if (!shortcut_features_container_->GetLayoutManager()) {
    views::BoxLayout* layout = shortcut_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(/*top=*/kDefaultVerticalMargin / 2, 0,
                        /*bottom=*/kMenuEdgeMargin, 0),
            kButtonSpacing));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
  }

  views::Button* button = shortcut_features_container_->AddChildView(
      CreateCircularImageButton(this, icon, text, /*show_border=*/true));

  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::AddFeatureButton(const gfx::ImageSkia& icon,
                                           const base::string16& text,
                                           base::RepeatingClosure action) {
  constexpr int kIconSize = 16;

  // Initialize layout if this is the first time a button is added.
  if (!features_container_->GetLayoutManager()) {
    features_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
  }

  views::Button* button =
      features_container_->AddChildView(std::make_unique<HoverButton>(
          this, SizeImage(ColorImage(icon, GetDefaultIconColor()), kIconSize),
          text));

  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::SetProfileManagementHeading(
    const base::string16& heading) {
  // Add separator before heading.
  profile_mgmt_separator_container_->RemoveAllChildViews(
      /*delete_children=*/true);
  profile_mgmt_separator_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  profile_mgmt_separator_container_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kDefaultVerticalMargin, /*horizontal=*/0)));
  profile_mgmt_separator_container_->AddChildView(
      std::make_unique<views::Separator>());

  // Initialize heading layout.
  profile_mgmt_heading_container_->RemoveAllChildViews(
      /*delete_children=*/true);
  profile_mgmt_heading_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  profile_mgmt_heading_container_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kDefaultVerticalMargin, kMenuEdgeMargin)));

  // Add heading.
  views::Label* label = profile_mgmt_heading_container_->AddChildView(
      std::make_unique<views::Label>(heading, views::style::CONTEXT_LABEL,
                                     STYLE_HINT));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetHandlesTooltips(false);
}

void ProfileMenuViewBase::AddSelectableProfile(const gfx::ImageSkia& image,
                                               const base::string16& name,
                                               bool is_guest,
                                               base::RepeatingClosure action) {
  constexpr int kImageSize = 22;

  // Initialize layout if this is the first time a button is added.
  if (!selectable_profiles_container_->GetLayoutManager()) {
    selectable_profiles_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
  }

  gfx::ImageSkia sized_image = CropCircle(SizeImage(image, kImageSize));
  views::Button* button = selectable_profiles_container_->AddChildView(
      std::make_unique<HoverButton>(this, sized_image, name));

  if (!is_guest && !first_profile_button_)
    first_profile_button_ = button;

  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::AddProfileManagementShortcutFeatureButton(
    const gfx::ImageSkia& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  // Initialize layout if this is the first time a button is added.
  if (!profile_mgmt_shortcut_features_container_->GetLayoutManager()) {
    profile_mgmt_shortcut_features_container_->SetLayoutManager(
        CreateBoxLayout(views::BoxLayout::Orientation::kHorizontal,
                        views::BoxLayout::CrossAxisAlignment::kCenter,
                        gfx::Insets(0, 0, 0, /*right=*/kMenuEdgeMargin)));
  }

  views::Button* button =
      profile_mgmt_shortcut_features_container_->AddChildView(
          CreateCircularImageButton(this, icon, text));

  RegisterClickAction(button, std::move(action));
}

void ProfileMenuViewBase::AddProfileManagementFeatureButton(
    const gfx::ImageSkia& icon,
    const base::string16& text,
    base::RepeatingClosure action) {
  constexpr int kIconSize = 22;

  // Initialize layout if this is the first time a button is added.
  if (!profile_mgmt_features_container_->GetLayoutManager()) {
    profile_mgmt_features_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
  }

  views::Button* button = profile_mgmt_features_container_->AddChildView(
      std::make_unique<HoverButton>(this, SizeImage(icon, kIconSize), text));

  RegisterClickAction(button, std::move(action));
}

gfx::ImageSkia ProfileMenuViewBase::ImageForMenu(const gfx::VectorIcon& icon,
                                                 float icon_to_image_ratio) {
  const int padding =
      static_cast<int>(kMaxImageSize * (1.0 - icon_to_image_ratio) / 2.0);

  auto sized_icon = gfx::CreateVectorIcon(icon, kMaxImageSize - 2 * padding,
                                          GetDefaultIconColor());
  return gfx::CanvasImageSource::CreatePadded(sized_icon, gfx::Insets(padding));
}

gfx::ImageSkia ProfileMenuViewBase::ColoredImageForMenu(
    const gfx::VectorIcon& icon,
    SkColor color) {
  return gfx::CreateVectorIcon(icon, kMaxImageSize, color);
}

void ProfileMenuViewBase::RecordClick(ActionableItem item) {
  // TODO(tangltom): Separate metrics for incognito and guest menu.
  base::UmaHistogramEnumeration("Profile.Menu.ClickedActionableItem", item);
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

void ProfileMenuViewBase::StyledLabelLinkClicked(views::StyledLabel* link,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  OnClick(link);
}

void ProfileMenuViewBase::OnClick(views::View* clickable_view) {
  DCHECK(!click_actions_[clickable_view].is_null());
  signin_ui_util::RecordProfileMenuClick(browser()->profile());
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
  click_actions_.clear();
  RemoveAllChildViews(/*delete_childen=*/true);

  auto components = std::make_unique<views::View>();
  components->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Create and add new component containers in the correct order.
  // First, add the parts of the current profile.
  heading_container_ =
      components->AddChildView(std::make_unique<views::View>());
  identity_info_container_ =
      components->AddChildView(std::make_unique<views::View>());
  shortcut_features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  sync_info_container_ =
      components->AddChildView(std::make_unique<views::View>());
  features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  profile_mgmt_separator_container_ =
      components->AddChildView(std::make_unique<views::View>());
  // Second, add the profile management header. This includes the heading and
  // the shortcut feature(s) next to it.
  auto profile_mgmt_header = std::make_unique<views::View>();
  views::BoxLayout* profile_mgmt_header_layout =
      profile_mgmt_header->SetLayoutManager(
          CreateBoxLayout(views::BoxLayout::Orientation::kHorizontal,
                          views::BoxLayout::CrossAxisAlignment::kCenter));
  profile_mgmt_heading_container_ =
      profile_mgmt_header->AddChildView(std::make_unique<views::View>());
  profile_mgmt_header_layout->SetFlexForView(profile_mgmt_heading_container_,
                                             1);
  profile_mgmt_shortcut_features_container_ =
      profile_mgmt_header->AddChildView(std::make_unique<views::View>());
  profile_mgmt_header_layout->SetFlexForView(
      profile_mgmt_shortcut_features_container_, 0);
  components->AddChildView(std::move(profile_mgmt_header));
  // Third, add the profile management buttons.
  selectable_profiles_container_ =
      components->AddChildView(std::make_unique<views::View>());
  profile_mgmt_features_container_ =
      components->AddChildView(std::make_unique<views::View>());
  first_profile_button_ = nullptr;

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

void ProfileMenuViewBase::RegisterClickAction(views::View* clickable_view,
                                              base::RepeatingClosure action) {
  DCHECK(click_actions_.count(clickable_view) == 0);
  click_actions_[clickable_view] = std::move(action);
}

void ProfileMenuViewBase::FocusButtonOnKeyboardOpen() {
  if (first_profile_button_)
    first_profile_button_->RequestFocus();
}
