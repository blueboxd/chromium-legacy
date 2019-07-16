// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/metrics/tab_count_metrics.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "components/tab_count_metrics/tab_count_metrics.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace {
// Maximum number of lines that a title label occupies.
int kTitleMaxLines = 2;

// Hover card and preview image dimensions.
int GetPreferredTabHoverCardWidth() {
  return TabStyle::GetStandardWidth();
}

gfx::Size GetTabHoverCardPreviewImageSize() {
  constexpr float kTabHoverCardPreviewImageAspectRatio = 16.0f / 9.0f;
  const int width = GetPreferredTabHoverCardWidth();
  return gfx::Size(width, width / kTabHoverCardPreviewImageAspectRatio);
}

bool AreHoverCardImagesEnabled() {
  return base::FeatureList::IsEnabled(features::kTabHoverCardImages);
}

// Get delay threshold based on flag settings option selected. This is for
// user testing.
// TODO(corising): remove this after user study is completed.
base::TimeDelta GetMinimumTriggerDelay() {
  int delay_group = base::GetFieldTrialParamByFeatureAsInt(
      features::kTabHoverCards, features::kTabHoverCardsFeatureParameterName,
      0);
  switch (delay_group) {
    case 2:
      return base::TimeDelta::FromMilliseconds(150);
    case 1:
      return base::TimeDelta::FromMilliseconds(200);
    case 0:
    default:
      return base::TimeDelta::FromMilliseconds(0);
  }
}

base::TimeDelta GetMaximumTriggerDelay() {
  int delay_group = base::GetFieldTrialParamByFeatureAsInt(
      features::kTabHoverCards, features::kTabHoverCardsFeatureParameterName,
      0);
  switch (delay_group) {
    case 2:
      return base::TimeDelta::FromMilliseconds(500);
    case 1:
      return base::TimeDelta::FromMilliseconds(700);
    case 0:
    default:
      return base::TimeDelta::FromMilliseconds(0);
  }
}

bool CustomShadowsSupported() {
#if defined(OS_WIN)
  return ui::win::IsAeroGlassEnabled();
#else
  return true;
#endif
}

}  // namespace

// static
bool TabHoverCardBubbleView::disable_animations_for_testing_ = false;

// TODO(corising): Move this to a place where it could be used for all widgets.
class TabHoverCardBubbleView::WidgetFadeAnimationDelegate
    : public views::AnimationDelegateViews {
 public:
  explicit WidgetFadeAnimationDelegate(views::Widget* hover_card)
      : AnimationDelegateViews(hover_card->GetRootView()),
        widget_(hover_card),
        fade_animation_(std::make_unique<gfx::LinearAnimation>(this)) {}
  ~WidgetFadeAnimationDelegate() override {}

  enum class FadeAnimationState {
    // No animation is running.
    IDLE,
    FADE_IN,
    FADE_OUT,
  };

  void set_animation_state(FadeAnimationState state) {
    animation_state_ = state;
  }

  bool IsFadingIn() const {
    return animation_state_ == FadeAnimationState::FADE_IN;
  }

  bool IsFadingOut() const {
    return animation_state_ == FadeAnimationState::FADE_OUT;
  }

  void FadeIn() {
    if (IsFadingIn())
      return;
    constexpr base::TimeDelta kFadeInDuration =
        base::TimeDelta::FromMilliseconds(200);
    set_animation_state(FadeAnimationState::FADE_IN);
    // Widgets cannot be shown when visible and fully transparent.
    widget_->SetOpacity(0.01f);
    widget_->Show();
    fade_animation_ = std::make_unique<gfx::LinearAnimation>(this);
    fade_animation_->SetDuration(kFadeInDuration);
    fade_animation_->Start();
  }

  void FadeOut() {
    if (IsFadingOut())
      return;
    constexpr base::TimeDelta kFadeOutDuration =
        base::TimeDelta::FromMilliseconds(150);
    fade_animation_ = std::make_unique<gfx::LinearAnimation>(this);
    set_animation_state(FadeAnimationState::FADE_OUT);
    fade_animation_->SetDuration(kFadeOutDuration);
    fade_animation_->Start();
  }

  void CancelFadeOut() {
    if (!IsFadingOut())
      return;

    fade_animation_->Stop();
    set_animation_state(FadeAnimationState::IDLE);
    widget_->SetOpacity(1.0f);
  }

 private:
  void AnimationProgressed(const gfx::Animation* animation) override {
    // Get the value of the animation with a material ease applied.
    double value = gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                              animation->GetCurrentValue());
    float opaqueness = 0.0f;
    if (IsFadingOut()) {
      opaqueness = gfx::Tween::FloatValueBetween(value, 1.0f, 0.0f);
    } else if (animation_state_ == FadeAnimationState::FADE_IN) {
      opaqueness = gfx::Tween::FloatValueBetween(value, 0.0f, 1.0f);
    }

    if (IsFadingOut() && opaqueness == 0.0f) {
      widget_->Hide();
    } else {
      widget_->SetOpacity(opaqueness);
    }
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    AnimationProgressed(animation);
    set_animation_state(FadeAnimationState::IDLE);
  }

  views::Widget* const widget_;
  std::unique_ptr<gfx::LinearAnimation> fade_animation_;
  FadeAnimationState animation_state_ = FadeAnimationState::IDLE;

  DISALLOW_COPY_AND_ASSIGN(WidgetFadeAnimationDelegate);
};

class TabHoverCardBubbleView::WidgetSlideAnimationDelegate
    : public views::AnimationDelegateViews {
 public:
  explicit WidgetSlideAnimationDelegate(
      TabHoverCardBubbleView* hover_card_delegate)
      : AnimationDelegateViews(hover_card_delegate),
        bubble_delegate_(hover_card_delegate),
        slide_animation_(std::make_unique<gfx::SlideAnimation>(this)) {
    constexpr int kSlideDuration = 75;
    slide_animation_->SetSlideDuration(kSlideDuration);
  }
  ~WidgetSlideAnimationDelegate() override {}

  void AnimateToAnchorView(views::View* desired_anchor_view) {
    DCHECK(!current_bubble_bounds_.IsEmpty());
    desired_anchor_view_ = desired_anchor_view;
    gfx::Rect anchor_bounds = desired_anchor_view->GetAnchorBoundsInScreen();
    anchor_bounds.Inset(bubble_delegate_->anchor_view_insets());

    starting_bubble_bounds_ = current_bubble_bounds_;

    slide_animation_->Reset(0);

    target_bubble_bounds_ =
        bubble_delegate_->GetBubbleFrameView()->GetUpdatedWindowBounds(
            anchor_bounds, bubble_delegate_->arrow(),
            bubble_delegate_->GetWidget()->client_view()->GetPreferredSize(),
            true);

    slide_animation_->Show();
  }

  void StopAnimation() { AnimationCanceled(slide_animation_.get()); }

  void SetBounds() {
    current_bubble_bounds_ = bubble_delegate_->GetBubbleBounds();
  }

  bool IsAnimating() { return slide_animation_->is_animating(); }

 private:
  void AnimationProgressed(const gfx::Animation* animation) override {
    double value = gfx::Tween::CalculateValue(
        gfx::Tween::FAST_OUT_SLOW_IN, slide_animation_->GetCurrentValue());
    current_bubble_bounds_ = gfx::Tween::RectValueBetween(
        value, starting_bubble_bounds_, target_bubble_bounds_);

    if (current_bubble_bounds_ == target_bubble_bounds_) {
      bubble_delegate_->SetAnchorView(desired_anchor_view_);
      AnimationEnded(animation);
    }
    bubble_delegate_->GetWidget()->SetBounds(current_bubble_bounds_);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    desired_anchor_view_ = nullptr;
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
    slide_animation_->Stop();
  }

  TabHoverCardBubbleView* const bubble_delegate_;
  std::unique_ptr<gfx::SlideAnimation> slide_animation_;
  views::View* desired_anchor_view_ = nullptr;
  gfx::Rect starting_bubble_bounds_;
  gfx::Rect target_bubble_bounds_;
  gfx::Rect current_bubble_bounds_;

  DISALLOW_COPY_AND_ASSIGN(WidgetSlideAnimationDelegate);
};

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab)
    : BubbleDialogDelegateView(tab, views::BubbleBorder::TOP_LEFT) {
  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Inset the tab hover cards anchor rect to bring the card closer to the tab.
  constexpr gfx::Insets kTabHoverCardAnchorInsets(2, 0);
  set_anchor_view_insets(kTabHoverCardAnchorInsets);

  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);

  // Set so that the tab hover card is not focus traversable when keyboard
  // navigating through the tab strip.
  set_focus_traversable_from_anchor_view(false);

  title_label_ =
      new views::Label(base::string16(), CONTEXT_TAB_HOVER_CARD_TITLE,
                       views::style::STYLE_PRIMARY);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetMultiLine(true);
  title_label_->SetMaxLines(kTitleMaxLines);
  AddChildView(title_label_);

  domain_label_ =
      new views::Label(base::string16(), CONTEXT_BODY_TEXT_LARGE,
                       ChromeTextStyle::STYLE_SECONDARY,
                       gfx::DirectionalityMode::DIRECTIONALITY_AS_URL);
  domain_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  domain_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  domain_label_->SetMultiLine(false);
  AddChildView(domain_label_);

  if (AreHoverCardImagesEnabled()) {
    using Alignment = views::ImageView::Alignment;
    preview_image_ = new views::ImageView();
    preview_image_->SetVisible(AreHoverCardImagesEnabled());
    preview_image_->SetHorizontalAlignment(Alignment::kCenter);
    preview_image_->SetVerticalAlignment(Alignment::kCenter);
    preview_image_->SetImageSize(GetTabHoverCardPreviewImageSize());
    preview_image_->SetPreferredSize(GetTabHoverCardPreviewImageSize());
    AddChildView(preview_image_);
  }

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);

  constexpr int kHorizontalMargin = 12;
  constexpr int kVerticalMargin = 18;
  constexpr int kLineSpacing = 0;
  title_label_->SetProperty(views::kMarginsKey,
                            gfx::Insets(kHorizontalMargin, kVerticalMargin,
                                        kLineSpacing, kVerticalMargin));
  domain_label_->SetProperty(views::kMarginsKey,
                             gfx::Insets(kLineSpacing, kVerticalMargin,
                                         kHorizontalMargin, kVerticalMargin));

  widget_ = views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);

  slide_animation_delegate_ =
      std::make_unique<WidgetSlideAnimationDelegate>(this);
  fade_animation_delegate_ =
      std::make_unique<WidgetFadeAnimationDelegate>(widget_);

  GetBubbleFrameView()->set_preferred_arrow_adjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  if (CustomShadowsSupported()) {
    GetBubbleFrameView()->SetCornerRadius(
        ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
            views::EMPHASIS_HIGH));
  }
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

void TabHoverCardBubbleView::UpdateAndShow(Tab* tab) {
  RecordTimeSinceLastSeenMetric(base::TimeTicks::Now() -
                                last_visible_timestamp_);
  // If less than |kShowWithoutDelayTimeBuffer| time has passed since the hover
  // card was last visible then it is shown immediately. This is to account for
  // if hover unintentionally leaves the tab strip.
  constexpr base::TimeDelta kShowWithoutDelayTimeBuffer =
      base::TimeDelta::FromMilliseconds(500);
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_mouse_exit_timestamp_;

  bool within_delay_time_buffer = !last_mouse_exit_timestamp_.is_null() &&
                                  elapsed_time <= kShowWithoutDelayTimeBuffer;
  // Hover cards should be shown without delay if triggered within the time
  // buffer or if the tab or its children have focus which indicates that the
  // tab is keyboard focused.
  const views::FocusManager* tab_focus_manager = tab->GetFocusManager();
  bool show_immediately =
      within_delay_time_buffer || tab->HasFocus() ||
      (tab_focus_manager && tab->Contains(tab_focus_manager->GetFocusedView()));

  fade_animation_delegate_->CancelFadeOut();

  if (preview_image_)
    preview_image_->SetVisible(!tab->IsActive());

  UpdateCardContent(tab);
  // If widget is already visible and anchored to the correct tab we should not
  // try to reset the anchor view or reshow.
  if (widget_->IsVisible() && GetAnchorView() == tab &&
      !slide_animation_delegate_->IsAnimating()) {
    return;
  }

  if (widget_->IsVisible())
    ++hover_cards_seen_count_;

  if (widget_->IsVisible() && !disable_animations_for_testing_) {
    slide_animation_delegate_->AnimateToAnchorView(tab);
  } else {
    SetAnchorView(tab);
    // Store the current bubble bounds now to be used when animating to a new
    // view. We do this now since the anchor view is needed to get bubble
    // bounds and could be deleting later when using the bounds to animate.
    slide_animation_delegate_->SetBounds();
  }

  if (!widget_->IsVisible()) {
    if (disable_animations_for_testing_ || show_immediately) {
      widget_->SetOpacity(1.0f);
      widget_->Show();
    } else {
      // Note that this will restart the timer if it is already running. If the
      // hover cards are not yet visible, moving the cursor within the tabstrip
      // will not trigger the hover cards.
      delayed_show_timer_.Start(FROM_HERE, GetDelay(tab->width()), this,
                                &TabHoverCardBubbleView::FadeInToShow);
    }
  }
}

void TabHoverCardBubbleView::FadeOutToHide() {
  delayed_show_timer_.Stop();
  if (!widget_->IsVisible())
    return;
  slide_animation_delegate_->StopAnimation();
  last_visible_timestamp_ = base::TimeTicks::Now();
  if (disable_animations_for_testing_) {
    widget_->Hide();
  } else {
    fade_animation_delegate_->FadeOut();
  }
}

bool TabHoverCardBubbleView::IsFadingOut() const {
  return fade_animation_delegate_->IsFadingOut();
}

void TabHoverCardBubbleView::RecordHoverCardsSeenRatioMetric() {
  const char kHistogramPrefixHoverCardsSeenBeforeSelection[] =
      "TabHoverCards.TabHoverCardsSeenBeforeTabSelection";
  const size_t tab_count = tab_count_metrics::TabCount();
  const size_t bucket = tab_count_metrics::BucketForTabCount(tab_count);
  constexpr int kMinHoverCardsSeen = 0;
  constexpr int kMaxHoverCardsSeen = 100;
  constexpr int kHistogramBucketCount = 50;
  STATIC_HISTOGRAM_POINTER_GROUP(
      tab_count_metrics::HistogramName(
          kHistogramPrefixHoverCardsSeenBeforeSelection,
          /* live_tabs_only */ false, bucket),
      static_cast<int>(bucket),
      static_cast<int>(tab_count_metrics::kNumTabCountBuckets),
      Add(hover_cards_seen_count_),
      base::Histogram::FactoryGet(
          tab_count_metrics::HistogramName(
              kHistogramPrefixHoverCardsSeenBeforeSelection,
              /* live_tabs_only */ false, bucket),
          kMinHoverCardsSeen, kMaxHoverCardsSeen, kHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

void TabHoverCardBubbleView::OnWidgetVisibilityChanged(views::Widget* widget,
                                                       bool visible) {
  if (visible)
    ++hover_cards_seen_count_;
}

int TabHoverCardBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

base::TimeDelta TabHoverCardBubbleView::GetDelay(int tab_width) const {
  // Delay is calculated as a logarithmic scale and bounded by a minimum width
  // based on the width of a pinned tab and a maximum of the standard width.
  //
  //  delay (ms)
  //           |
  // max delay-|                                    *
  //           |                          *
  //           |                    *
  //           |                *
  //           |            *
  //           |         *
  //           |       *
  //           |     *
  //           |    *
  // min delay-|****
  //           |___________________________________________ tab width
  //               |                                |
  //       pinned tab width               standard tab width
  base::TimeDelta minimum_trigger_delay = GetMinimumTriggerDelay();
  if (tab_width < TabStyle::GetPinnedWidth())
    return minimum_trigger_delay;
  base::TimeDelta maximum_trigger_delay = GetMaximumTriggerDelay();
  double logarithmic_fraction =
      std::log(tab_width - TabStyle::GetPinnedWidth() + 1) /
      std::log(TabStyle::GetStandardWidth() - TabStyle::GetPinnedWidth() + 1);
  base::TimeDelta scaling_factor =
      maximum_trigger_delay - minimum_trigger_delay;
  base::TimeDelta delay =
      logarithmic_fraction * scaling_factor + minimum_trigger_delay;
  return delay;
}

void TabHoverCardBubbleView::FadeInToShow() {
  fade_animation_delegate_->FadeIn();
}

void TabHoverCardBubbleView::UpdateCardContent(const Tab* tab) {
  title_label_->SetText(tab->data().title);

  base::string16 domain = url_formatter::FormatUrl(
      tab->data().last_committed_url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlTrimAfterHost,
      net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
  domain_label_->SetText(domain);

  // If the preview image feature is not enabled, |preview_image_| will be null.
  if (preview_image_ && preview_image_->GetVisible()) {
    // If there is no valid thumbnail data, blank out the preview, else wait for
    // the image data to be decoded and update momentarily.
    if (!tab->data().thumbnail.AsImageSkiaAsync(
            base::BindOnce(&TabHoverCardBubbleView::UpdatePreviewImage,
                           weak_factory_.GetWeakPtr()))) {
      // Check the no-preview color and size to see if it needs to be
      // regenerated. DPI or theme change can cause a regeneration.
      const SkColor foreground_color = tab->GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_FOREGROUND);

      // Set the no-preview placeholder image. All sizes are in DIPs.
      // gfx::CreateVectorIcon() caches its result so there's no need to store
      // images here; if a particular size/color combination has already been
      // requested it will be low-cost to request it again.
      constexpr gfx::Size kNoPreviewImageSize{64, 64};
      const gfx::ImageSkia no_preview_image = gfx::CreateVectorIcon(
          kGlobeIcon, kNoPreviewImageSize.width(), foreground_color);
      preview_image_->SetImage(no_preview_image);
      preview_image_->SetImageSize(kNoPreviewImageSize);
      preview_image_->SetPreferredSize(GetTabHoverCardPreviewImageSize());

      // Also possibly regenerate the background if it has changed.
      const SkColor background_color = tab->GetThemeProvider()->GetColor(
          ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_BACKGROUND);
      if (!preview_image_->background() ||
          preview_image_->background()->get_color() != background_color) {
        preview_image_->SetBackground(
            views::CreateSolidBackground(background_color));
      }
    }
  }
}

void TabHoverCardBubbleView::UpdatePreviewImage(gfx::ImageSkia preview_image) {
  preview_image_->SetImage(preview_image);
  preview_image_->SetImageSize(GetTabHoverCardPreviewImageSize());
  preview_image_->SetPreferredSize(GetTabHoverCardPreviewImageSize());
  preview_image_->SetBackground(nullptr);
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize() const {
  gfx::Size preferred_size = GetLayoutManager()->GetPreferredSize(this);
  preferred_size.set_width(GetPreferredTabHoverCardWidth());
  DCHECK(!preferred_size.IsEmpty());
  return preferred_size;
}

void TabHoverCardBubbleView::RecordTimeSinceLastSeenMetric(
    base::TimeDelta elapsed_time) {
  constexpr base::TimeDelta kMaxHoverCardReshowTimeDelta =
      base::TimeDelta::FromSeconds(5);
  if ((!widget_->IsVisible() || IsFadingOut()) &&
      elapsed_time <= kMaxHoverCardReshowTimeDelta) {
    constexpr base::TimeDelta kMinHoverCardReshowTimeDelta =
        base::TimeDelta::FromMilliseconds(1);
    constexpr int kHoverCardHistogramBucketCount = 50;
    UMA_HISTOGRAM_CUSTOM_TIMES("TabHoverCards.TimeSinceLastVisible",
                               elapsed_time, kMinHoverCardReshowTimeDelta,
                               kMaxHoverCardReshowTimeDelta,
                               kHoverCardHistogramBucketCount);
  }
}
