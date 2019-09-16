// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_view.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/web_apps/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/web_app_origin_text.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/views/window/hit_test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/terminal_system_app_menu_button_chromeos.h"
#endif

namespace {

bool g_animation_disabled_for_testing = false;

constexpr base::TimeDelta kContentSettingsFadeInDuration =
    base::TimeDelta::FromMilliseconds(500);

class WebAppToolbarActionsBar : public ToolbarActionsBar {
 public:
  using ToolbarActionsBar::ToolbarActionsBar;

  gfx::Insets GetIconAreaInsets() const override {
    // TODO(calamity): Unify these toolbar action insets with other clients once
    // all toolbar button sizings are consolidated. https://crbug.com/822967.
    return gfx::Insets(2);
  }

  size_t GetIconCount() const override {
    // Only show an icon when an extension action is popped out due to
    // activation, and none otherwise.
    return GetPoppedOutAction() ? 1 : 0;
  }

  int GetMinimumWidth() const override {
    // Allow the BrowserActionsContainer to collapse completely and be hidden
    return 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppToolbarActionsBar);
};

int HorizontalPaddingBetweenItems() {
  return views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
}

}  // namespace

const char WebAppFrameToolbarView::kViewClassName[] = "WebAppFrameToolbarView";

constexpr base::TimeDelta WebAppFrameToolbarView::kTitlebarAnimationDelay;
constexpr base::TimeDelta WebAppFrameToolbarView::kOriginFadeInDuration;
constexpr base::TimeDelta WebAppFrameToolbarView::kOriginPauseDuration;
constexpr base::TimeDelta WebAppFrameToolbarView::kOriginFadeOutDuration;

// static
base::TimeDelta WebAppFrameToolbarView::OriginTotalDuration() {
  // TimeDelta.operator+ uses time_internal::SaturatedAdd() which isn't
  // constexpr, so this needs to be a function to not introduce a static
  // initializer.
  return kOriginFadeInDuration + kOriginPauseDuration + kOriginFadeOutDuration;
}

class WebAppFrameToolbarView::ContentSettingsContainer : public views::View {
 public:
  explicit ContentSettingsContainer(
      ContentSettingImageView::Delegate* delegate);
  ~ContentSettingsContainer() override = default;

  void UpdateContentSettingViewsVisibility() {
    for (auto* v : content_setting_views_)
      v->Update();
  }

  // Sets the color of the content setting icons.
  void SetIconColor(SkColor icon_color) {
    for (auto* v : content_setting_views_)
      v->SetIconColor(icon_color);
  }

  void SetUpForFadeIn() {
    SetVisible(false);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(0);
  }

  void FadeIn() {
    if (GetVisible())
      return;
    SetVisible(true);
    DCHECK_EQ(layer()->opacity(), 0);
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(kContentSettingsFadeInDuration);
    layer()->SetOpacity(1);
  }

  void EnsureVisible() {
    SetVisible(true);
    if (layer())
      layer()->SetOpacity(1);
  }

  const std::vector<ContentSettingImageView*>&
  GetContentSettingViewsForTesting() const {
    return content_setting_views_;
  }

 private:
  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  // Owned by the views hierarchy.
  std::vector<ContentSettingImageView*> content_setting_views_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsContainer);
};

WebAppFrameToolbarView::ContentSettingsContainer::ContentSettingsContainer(
    ContentSettingImageView::Delegate* delegate) {
  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          views::LayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      ContentSettingImageModel::GenerateContentSettingImageModels();
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), delegate,
        views::CustomFrameView::GetWindowTitleFontList());
    // Padding around content setting icons.
    constexpr int kContentSettingIconInteriorPadding = 4;
    image_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kContentSettingIconInteriorPadding)));
    image_view->disable_animation();
    views::SetHitTestComponent(image_view.get(), static_cast<int>(HTCLIENT));
    content_setting_views_.push_back(image_view.get());
    AddChildView(image_view.release());
  }
}

WebAppFrameToolbarView::WebAppFrameToolbarView(views::Widget* widget,
                                               BrowserView* browser_view,
                                               SkColor active_color,
                                               SkColor inactive_color,
                                               base::Optional<int> right_margin)
    : browser_view_(browser_view),
      active_color_(active_color),
      inactive_color_(inactive_color) {
  DCHECK(browser_view_);
  DCHECK(web_app::AppBrowserController::IsForWebAppBrowser(
      browser_view_->browser()));

  SetID(VIEW_ID_WEB_APP_FRAME_TOOLBAR);

  views::BoxLayout& layout =
      *SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0,
                      right_margin.value_or(HorizontalPaddingBetweenItems())),
          HorizontalPaddingBetweenItems()));
  // Right align to clip the leftmost items first when not enough space.
  layout.set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout.set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* app_controller = browser_view_->browser()->app_controller();
  if (app_controller->HasTitlebarAppOriginText()) {
    web_app_origin_text_ = AddChildView(
        std::make_unique<WebAppOriginText>(browser_view->browser()));
  }

  if (app_controller->HasTitlebarContentSettings()) {
    content_settings_container_ =
        AddChildView(std::make_unique<ContentSettingsContainer>(this));
    views::SetHitTestComponent(content_settings_container_,
                               static_cast<int>(HTCLIENT));
  }

  OmniboxPageActionIconContainerView::Params params;
  params.types_enabled.push_back(PageActionIconType::kFind);
  params.types_enabled.push_back(PageActionIconType::kManagePasswords);
  params.types_enabled.push_back(PageActionIconType::kTranslate);
  params.types_enabled.push_back(PageActionIconType::kZoom);
  if (base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI))
    params.types_enabled.push_back(PageActionIconType::kNativeFileSystemAccess);
  if (base::FeatureList::IsEnabled(content_settings::kImprovedCookieControls))
    params.types_enabled.push_back(PageActionIconType::kCookieControls);
  params.types_enabled.push_back(PageActionIconType::kLocalCardMigration);
  params.types_enabled.push_back(PageActionIconType::kSaveCard);
  params.icon_size = GetLayoutConstant(WEB_APP_PAGE_ACTION_ICON_SIZE);
  params.icon_color = GetCaptionColor();
  params.between_icon_spacing = HorizontalPaddingBetweenItems();
  params.browser = browser_view_->browser();
  params.command_updater = browser_view_->browser()->command_controller();
  params.page_action_icon_delegate = this;
  omnibox_page_action_icon_container_view_ = AddChildView(
      std::make_unique<OmniboxPageActionIconContainerView>(params));
  views::SetHitTestComponent(omnibox_page_action_icon_container_view_,
                             static_cast<int>(HTCLIENT));

  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    extensions_container_ = AddChildView(
        std::make_unique<ExtensionsToolbarContainer>(browser_view_->browser()));
    views::SetHitTestComponent(extensions_container_,
                               static_cast<int>(HTCLIENT));
  } else {
    browser_actions_container_ =
        AddChildView(std::make_unique<BrowserActionsContainer>(
            browser_view->browser(), nullptr, this, false /* interactive */));
    views::SetHitTestComponent(browser_actions_container_,
                               static_cast<int>(HTCLIENT));
  }

// TODO(crbug.com/998900): Create AppControllerUi class to contain this logic.
#if defined(OS_CHROMEOS)
  if (app_controller->UseTitlebarTerminalSystemAppMenu()) {
    web_app_menu_button_ = AddChildView(
        std::make_unique<TerminalSystemAppMenuButton>(browser_view));
  } else {
    web_app_menu_button_ =
        AddChildView(std::make_unique<WebAppMenuButton>(browser_view));
  }
#else
  web_app_menu_button_ =
      AddChildView(std::make_unique<WebAppMenuButton>(browser_view));
#endif

  UpdateChildrenColor();
  UpdateStatusIconsVisibility();

  DCHECK(!browser_view_->toolbar_button_provider() ||
         browser_view_->toolbar_button_provider()
                 ->GetAsAccessiblePaneView()
                 ->GetClassName() == GetClassName())
      << "This should be the first ToolbarButtorProvider or a replacement for "
         "an existing instance of this class during a window frame refresh.";
  browser_view_->SetToolbarButtonProvider(this);
  browser_view_->immersive_mode_controller()->AddObserver(this);
  scoped_widget_observer_.Add(widget);
}

WebAppFrameToolbarView::~WebAppFrameToolbarView() {
  ImmersiveModeController* immersive_controller =
      browser_view_->immersive_mode_controller();
  if (immersive_controller)
    immersive_controller->RemoveObserver(this);
}

void WebAppFrameToolbarView::UpdateStatusIconsVisibility() {
  if (content_settings_container_)
    content_settings_container_->UpdateContentSettingViewsVisibility();
  omnibox_page_action_icon_container_view_->UpdateAll();
}

void WebAppFrameToolbarView::UpdateCaptionColors() {
  const BrowserNonClientFrameView* frame_view =
      browser_view_->frame()->GetFrameView();
  active_color_ = frame_view->GetCaptionColor(
      BrowserNonClientFrameView::ActiveState::kActive);
  inactive_color_ = frame_view->GetCaptionColor(
      BrowserNonClientFrameView::ActiveState::kInactive);
  UpdateChildrenColor();
}

void WebAppFrameToolbarView::SetPaintAsActive(bool active) {
  if (paint_as_active_ == active)
    return;
  paint_as_active_ = active;
  UpdateChildrenColor();
}

int WebAppFrameToolbarView::LayoutInContainer(int leading_x,
                                              int trailing_x,
                                              int y,
                                              int available_height) {
  if (available_height == 0) {
    SetSize(gfx::Size());
    return trailing_x;
  }

  gfx::Size preferred_size = GetPreferredSize();
  const int width =
      base::ClampToRange(trailing_x - leading_x, 0, preferred_size.width());
  const int height = preferred_size.height();
  DCHECK_LE(height, available_height);
  SetBounds(trailing_x - width, y + (available_height - height) / 2, width,
            height);
  Layout();
  return bounds().x();
}

const char* WebAppFrameToolbarView::GetClassName() const {
  return kViewClassName;
}

views::LabelButton* WebAppFrameToolbarView::GetOverflowReferenceView() {
  return web_app_menu_button_;
}

base::Optional<int> WebAppFrameToolbarView::GetMaxBrowserActionsWidth() const {
  // Our maximum size is 1 icon so don't specify a pixel-width max here.
  return base::Optional<int>();
}

bool WebAppFrameToolbarView::CanShowIconInToolbar() const {
  return false;
}

std::unique_ptr<ToolbarActionsBar>
WebAppFrameToolbarView::CreateToolbarActionsBar(
    ToolbarActionsBarDelegate* delegate,
    Browser* browser,
    ToolbarActionsBar* main_bar) const {
  DCHECK_EQ(browser_view_->browser(), browser);
  return std::make_unique<WebAppToolbarActionsBar>(delegate, browser, main_bar);
}

SkColor WebAppFrameToolbarView::GetContentSettingInkDropColor() const {
  return GetCaptionColor();
}

content::WebContents* WebAppFrameToolbarView::GetContentSettingWebContents() {
  return browser_view_->GetActiveWebContents();
}

ContentSettingBubbleModelDelegate*
WebAppFrameToolbarView::GetContentSettingBubbleModelDelegate() {
  return browser_view_->browser()->content_setting_bubble_model_delegate();
}

void WebAppFrameToolbarView::OnContentSettingImageBubbleShown(
    ContentSettingImageModel::ImageType type) const {
  UMA_HISTOGRAM_ENUMERATION(
      "HostedAppFrame.ContentSettings.ImagePressed", type,
      ContentSettingImageModel::ImageType::NUM_IMAGE_TYPES);
}

void WebAppFrameToolbarView::OnImmersiveRevealStarted() {
  // Don't wait for the fade in animation to make content setting icons visible
  // once in immersive mode.
  if (content_settings_container_)
    content_settings_container_->EnsureVisible();
}

SkColor WebAppFrameToolbarView::GetPageActionInkDropColor() const {
  return GetCaptionColor();
}

content::WebContents*
WebAppFrameToolbarView::GetWebContentsForPageActionIconView() {
  return browser_view_->GetActiveWebContents();
}

BrowserActionsContainer* WebAppFrameToolbarView::GetBrowserActionsContainer() {
  CHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  return browser_actions_container_;
}

ToolbarActionView* WebAppFrameToolbarView::GetToolbarActionViewForId(
    const std::string& id) {
  // TODO(pbos): Implement this for kExtensionsToolbarMenu.
  CHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  return browser_actions_container_->GetViewForId(id);
}

views::View* WebAppFrameToolbarView::GetDefaultExtensionDialogAnchorView() {
  // TODO(pbos): Implement this for kExtensionsToolbarMenu.
  CHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  return GetAppMenuButton();
}

PageActionIconView* WebAppFrameToolbarView::GetPageActionIconView(
    PageActionIconType type) {
  return omnibox_page_action_icon_container_view_->GetIconView(type);
}

AppMenuButton* WebAppFrameToolbarView::GetAppMenuButton() {
  return web_app_menu_button_;
}

gfx::Rect WebAppFrameToolbarView::GetFindBarBoundingBox(
    int contents_height) const {
  if (!IsDrawn())
    return gfx::Rect();

  gfx::Rect anchor_bounds = web_app_menu_button_->ConvertRectToWidget(
      web_app_menu_button_->GetLocalBounds());
  if (base::i18n::IsRTL()) {
    // Find bar will be left aligned so align to left edge of app menu button.
    int widget_width = GetWidget()->GetRootView()->width();
    return gfx::Rect(anchor_bounds.x(), anchor_bounds.bottom(),
                     widget_width - anchor_bounds.x(), contents_height);
  }
  // Find bar will be right aligned so align to right edge of app menu button.
  return gfx::Rect(0, anchor_bounds.bottom(),
                   anchor_bounds.x() + anchor_bounds.width(), contents_height);
}

void WebAppFrameToolbarView::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* WebAppFrameToolbarView::GetAsAccessiblePaneView() {
  return this;
}

views::View* WebAppFrameToolbarView::GetAnchorView(PageActionIconType type) {
  return web_app_menu_button_;
}

void WebAppFrameToolbarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  omnibox_page_action_icon_container_view_->ZoomChangedForActiveTab(
      can_show_bubble);
}

void WebAppFrameToolbarView::OnWidgetVisibilityChanged(views::Widget* widget,
                                                       bool visibility) {
  if (!visibility || !pending_widget_visibility_)
    return;
  pending_widget_visibility_ = false;
  if (ShouldAnimate()) {
    if (content_settings_container_)
      content_settings_container_->SetUpForFadeIn();
    animation_start_delay_.Start(
        FROM_HERE, kTitlebarAnimationDelay, this,
        &WebAppFrameToolbarView::StartTitlebarAnimation);
  }
}

void WebAppFrameToolbarView::DisableAnimationForTesting() {
  g_animation_disabled_for_testing = true;
}

views::View* WebAppFrameToolbarView::GetPageActionIconContainerForTesting() {
  return omnibox_page_action_icon_container_view_;
}

gfx::Size WebAppFrameToolbarView::CalculatePreferredSize() const {
  // Prefer height consistency over accommodating edge case icons that may bump
  // up the container height (e.g. extension action icons with badges).
  // TODO(https://crbug.com/889745): Fix the inconsistent icon sizes found in
  // this container and turn this into a DCHECK that the conatiner height is the
  // same as the app menu button height.
  return gfx::Size(views::View::CalculatePreferredSize().width(),
                   web_app_menu_button_->GetPreferredSize().height());
}

void WebAppFrameToolbarView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void WebAppFrameToolbarView::ChildVisibilityChanged(views::View* child) {
  // Changes to layout need to be taken into account by the frame view.
  PreferredSizeChanged();
}

bool WebAppFrameToolbarView::ShouldAnimate() const {
  return !g_animation_disabled_for_testing &&
         !browser_view_->immersive_mode_controller()->IsEnabled();
}

void WebAppFrameToolbarView::StartTitlebarAnimation() {
  if (!ShouldAnimate())
    return;

  if (web_app_origin_text_)
    web_app_origin_text_->StartFadeAnimation();
  web_app_menu_button_->StartHighlightAnimation();
  icon_fade_in_delay_.Start(FROM_HERE, OriginTotalDuration(), this,
                            &WebAppFrameToolbarView::FadeInContentSettingIcons);
}

void WebAppFrameToolbarView::FadeInContentSettingIcons() {
  if (content_settings_container_)
    content_settings_container_->FadeIn();
}

views::View* WebAppFrameToolbarView::GetContentSettingContainerForTesting() {
  return content_settings_container_;
}

const std::vector<ContentSettingImageView*>&
WebAppFrameToolbarView::GetContentSettingViewsForTesting() const {
  return content_settings_container_->GetContentSettingViewsForTesting();
}

SkColor WebAppFrameToolbarView::GetCaptionColor() const {
  return paint_as_active_ ? active_color_ : inactive_color_;
}

void WebAppFrameToolbarView::UpdateChildrenColor() {
  SkColor icon_color = GetCaptionColor();
  if (web_app_origin_text_)
    web_app_origin_text_->SetTextColor(icon_color);
  if (content_settings_container_)
    content_settings_container_->SetIconColor(icon_color);
  omnibox_page_action_icon_container_view_->SetIconColor(icon_color);
  web_app_menu_button_->SetColor(icon_color);
}
