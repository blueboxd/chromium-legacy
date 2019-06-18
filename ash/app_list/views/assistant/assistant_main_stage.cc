// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_main_stage.h"

#include <numeric>

#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/base/stack_layout.h"
#include "ash/assistant/ui/main_stage/assistant_footer_view.h"
#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace app_list {

namespace {

// Appearance.
constexpr int kSeparatorThicknessDip = 1;
constexpr int kSeparatorWidthDip = 64;

// Footer animation.
constexpr int kFooterAnimationTranslationDip = 22;
constexpr base::TimeDelta kFooterAnimationTranslationDelay =
    base::TimeDelta::FromMilliseconds(66);
constexpr base::TimeDelta kFooterAnimationTranslationDuration =
    base::TimeDelta::FromMilliseconds(416);
constexpr base::TimeDelta kFooterAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(149);
constexpr base::TimeDelta kFooterAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kFooterAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(100);

// Footer entry animation.
constexpr base::TimeDelta kFooterEntryAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(283);
constexpr base::TimeDelta kFooterEntryAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);

// Divider animation.
constexpr base::TimeDelta kDividerAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(233);
constexpr base::TimeDelta kDividerAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kDividerAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);

// Greeting animation.
constexpr base::TimeDelta kGreetingAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);

// HorizontalSeparator ---------------------------------------------------------

// A horizontal line to separate the dialog plate.
class HorizontalSeparator : public views::View {
 public:
  explicit HorizontalSeparator(int preferred_width, int preferred_height)
      : preferred_width_(preferred_width),
        preferred_height_(preferred_height) {}

  ~HorizontalSeparator() override = default;

  // views::View overrides:
  const char* GetClassName() const override { return "HorizontalSeparator"; }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(preferred_width_, preferred_height_);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect draw_bounds(GetContentsBounds());
    draw_bounds.Inset(0, (draw_bounds.height() - kSeparatorThicknessDip) / 2);
    canvas->FillRect(draw_bounds, gfx::kGoogleGrey300);
  }

 private:
  const int preferred_width_;
  const int preferred_height_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalSeparator);
};

bool IsLayerVisible(views::View* view) {
  DCHECK(view->layer());
  return !cc::MathUtil::IsWithinEpsilon(view->layer()->GetTargetOpacity(), 0.f);
}

}  // namespace

// AssistantMainStage ----------------------------------------------------------

AssistantMainStage::AssistantMainStage(ash::AssistantViewDelegate* delegate)
    : delegate_(delegate),
      footer_animation_observer_(
          std::make_unique<ui::CallbackLayerAnimationObserver>(
              /*animation_started_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnFooterAnimationStarted,
                  base::Unretained(this)),
              /*animation_ended_callback=*/base::BindRepeating(
                  &AssistantMainStage::OnFooterAnimationEnded,
                  base::Unretained(this)))) {
  InitLayout();

  // The view hierarchy will be destructed before AssistantController in Shell,
  // which owns AssistantViewDelegate, so AssistantViewDelegate is guaranteed to
  // outlive the AssistantMainStage.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

AssistantMainStage::~AssistantMainStage() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* AssistantMainStage::GetClassName() const {
  return "AssistantMainStage";
}

void AssistantMainStage::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewPreferredSizeChanged(views::View* view) {
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void AssistantMainStage::InitLayout() {
  // The children of AssistantMainStage will be animated on their own layers and
  // we want them to be clipped by their parent layer.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* content_layout_container = CreateContentLayoutContainer();
  AddChildView(content_layout_container);
  layout->SetFlexForView(content_layout_container, 1);

  AddChildView(CreateFooterLayoutContainer());
}

views::View* AssistantMainStage::CreateContentLayoutContainer() {
  // The content layout container stacks two views.
  // On top is a main content container including the line separator, progress
  // indicator query view and |ui_element_container_|.
  // |greeting_label_| is laid out beneath of the main content container. As
  // such, it appears underneath and does not cause repositioning to any of
  // content layout's underlying views.
  views::View* content_layout_container = new views::View();

  InitGreetingLabel();
  content_layout_container->AddChildView(greeting_label_);
  auto* stack_layout = content_layout_container->SetLayoutManager(
      std::make_unique<ash::StackLayout>());

  // We need to stretch |greeting_label_| to match its parent so that it
  // won't use heuristics in Label to infer line breaking, which seems to cause
  // text clipping with DPI adjustment. See b/112843496.
  stack_layout->SetRespectDimensionForView(
      greeting_label_, ash::StackLayout::RespectDimension::kHeight);
  stack_layout->SetVerticalAlignmentForView(
      greeting_label_, ash::StackLayout::VerticalAlignment::kCenter);

  auto* main_content_layout_container = CreateMainContentLayoutContainer();
  content_layout_container->AddChildView(main_content_layout_container);

  // Do not respect height, otherwise bounds will not be set correctly for
  // scrolling.
  stack_layout->SetRespectDimensionForView(
      main_content_layout_container,
      ash::StackLayout::RespectDimension::kWidth);

  return content_layout_container;
}

void AssistantMainStage::InitGreetingLabel() {
  // Greeting label, which will be animated on its own layer.
  greeting_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
  greeting_label_->SetAutoColorReadabilityEnabled(false);
  greeting_label_->SetEnabledColor(ash::kTextColorPrimary);
  greeting_label_->SetFontList(
      ash::assistant::ui::GetDefaultFontList()
          .DeriveWithSizeDelta(8)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  greeting_label_->SetMultiLine(true);
  greeting_label_->SetPaintToLayer();
  greeting_label_->layer()->SetFillsBoundsOpaquely(false);
}

views::View* AssistantMainStage::CreateMainContentLayoutContainer() {
  views::View* content_layout_container = new views::View();
  views::BoxLayout* content_layout = content_layout_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  content_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  content_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  content_layout_container->AddChildView(CreateDividerLayoutContainer());

  // Query view. Will be animated on its own layer.
  query_view_ = new ash::AssistantQueryView();
  query_view_->SetPaintToLayer();
  query_view_->layer()->SetFillsBoundsOpaquely(false);
  query_view_->AddObserver(this);
  content_layout_container->AddChildView(query_view_);

  // UI element container.
  ui_element_container_ = new ash::UiElementContainerView(delegate_);
  ui_element_container_->AddObserver(this);
  content_layout_container->AddChildView(ui_element_container_);
  content_layout->SetFlexForView(ui_element_container_, 1,
                                 /*use_min_size=*/true);

  return content_layout_container;
}

views::View* AssistantMainStage::CreateDividerLayoutContainer() {
  // Dividers: the progress indicator and the horizontal separator will be the
  // separator when querying and showing the results, respectively.
  views::View* divider_container = new views::View();
  divider_container->SetLayoutManager(std::make_unique<ash::StackLayout>());

  // Progress indicator, which will be animated on its own layer.
  progress_indicator_ = new ash::AssistantProgressIndicator();
  progress_indicator_->SetPaintToLayer();
  progress_indicator_->layer()->SetFillsBoundsOpaquely(false);
  divider_container->AddChildView(progress_indicator_);

  // Horizontal separator, which will be animated on its own layer.
  horizontal_separator_ = new HorizontalSeparator(
      kSeparatorWidthDip, progress_indicator_->GetPreferredSize().height());
  horizontal_separator_->SetPaintToLayer();
  horizontal_separator_->layer()->SetFillsBoundsOpaquely(false);
  divider_container->AddChildView(horizontal_separator_);

  return divider_container;
}

views::View* AssistantMainStage::CreateFooterLayoutContainer() {
  // Footer.
  // Note that the |footer_| is placed within its own view container so that as
  // its visibility changes, its parent container will still reserve the same
  // layout space. This prevents jank that would otherwise occur due to
  // |ui_element_container_| claiming that empty space.
  views::View* footer_container = new views::View();
  footer_container->SetLayoutManager(std::make_unique<views::FillLayout>());

  footer_ = new ash::AssistantFooterView(delegate_);
  footer_->AddObserver(this);

  // The footer will be animated on its own layer.
  footer_->SetPaintToLayer();
  footer_->layer()->SetFillsBoundsOpaquely(false);

  footer_container->AddChildView(footer_);

  return footer_container;
}

void AssistantMainStage::OnCommittedQueryChanged(
    const ash::AssistantQuery& query) {
  // Update the view.
  query_view_->SetQuery(query);

  // If query is empty and we are showing greeting label, do not update the Ui.
  if (query.Empty() && IsLayerVisible(greeting_label_))
    return;

  using ash::assistant::util::CreateLayerAnimationSequence;
  using ash::assistant::util::CreateOpacityElement;

  // Hide the horizontal separator.
  horizontal_separator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(ash::assistant::util::CreateOpacityElement(
          0.f, kDividerAnimationFadeOutDuration)));

  // Show the progress indicator.
  progress_indicator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          // Delay...
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kDividerAnimationFadeInDelay),
          // ...then fade in.
          ash::assistant::util::CreateOpacityElement(
              1.f, kDividerAnimationFadeInDuration)));

  MaybeHideGreetingLabel();
}

void AssistantMainStage::OnPendingQueryChanged(
    const ash::AssistantQuery& query) {
  // Update the view.
  query_view_->SetQuery(query);
  UpdateFooter(/*visible=*/false);

  if (!IsLayerVisible(greeting_label_))
    return;

  using ash::assistant::util::CreateLayerAnimationSequence;
  using ash::assistant::util::CreateOpacityElement;

  // Animate the opacity to 100% with delay equal to |greeting_label_| fade out
  // animation duration to avoid the two views displaying at the same time.
  constexpr base::TimeDelta kQueryAnimationFadeInDuration =
      base::TimeDelta::FromMilliseconds(433);
  query_view_->layer()->SetOpacity(0.f);
  query_view_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kGreetingAnimationFadeOutDuration),
          CreateOpacityElement(1.f, kQueryAnimationFadeInDuration)));

  if (!query.Empty())
    MaybeHideGreetingLabel();
}

void AssistantMainStage::OnPendingQueryCleared(bool due_to_commit) {
  // When a pending query is cleared, it may be because the interaction was
  // cancelled, or because the query was committed. If the query was committed,
  // reseting the query here will have no visible effect. If the interaction was
  // cancelled, we set the query here to restore the previously committed query.
  query_view_->SetQuery(delegate_->GetInteractionModel()->committed_query());

  // If the query was committed, we wait for |OnResponseChanged()| to update the
  // footer. If the interaction was cancelled, we restore the previous
  // suggestion chips.
  if (!due_to_commit)
    UpdateFooter(/*visible=*/true);
}

void AssistantMainStage::OnResponseChanged(
    const std::shared_ptr<ash::AssistantResponse>& response) {
  MaybeHideGreetingLabel();

  using ash::assistant::util::CreateLayerAnimationSequence;
  using ash::assistant::util::CreateOpacityElement;

  // Show the horizontal separator.
  horizontal_separator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          // Delay...
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kDividerAnimationFadeInDelay),
          // ...then fade in.
          ash::assistant::util::CreateOpacityElement(
              1.f, kDividerAnimationFadeInDuration)));

  // Hide the progress indicator.
  progress_indicator_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(ash::assistant::util::CreateOpacityElement(
          0.f, kDividerAnimationFadeOutDuration)));

  UpdateFooter(/*visible=*/true);
}

void AssistantMainStage::OnUiVisibilityChanged(
    ash::AssistantVisibility new_visibility,
    ash::AssistantVisibility old_visibility,
    base::Optional<ash::AssistantEntryPoint> entry_point,
    base::Optional<ash::AssistantExitPoint> exit_point) {
  if (ash::assistant::util::IsStartingSession(new_visibility, old_visibility)) {
    // When Assistant is starting a new session, we animate in the appearance of
    // the greeting label and footer.
    using ash::assistant::util::CreateLayerAnimationSequence;
    using ash::assistant::util::CreateOpacityElement;
    using ash::assistant::util::CreateTransformElement;

    const bool from_search =
        entry_point == ash::AssistantEntryPoint::kLauncherSearchResult;
    progress_indicator_->layer()->SetOpacity(0.f);
    horizontal_separator_->layer()->SetOpacity(from_search ? 1.f : 0.f);

    greeting_label_->layer()->GetAnimator()->StopAnimating();

    // We're going to animate the greeting label up into position so we'll
    // need to apply an initial transformation.
    constexpr int kGreetingAnimationTranslationDip = 115;
    gfx::Transform transform;
    transform.Translate(0, kGreetingAnimationTranslationDip);

    // Set up our pre-animation values.
    greeting_label_->layer()->SetOpacity(0.f);
    greeting_label_->layer()->SetTransform(transform);
    if (!from_search) {
      constexpr base::TimeDelta kGreetingAnimationFadeInDelay =
          base::TimeDelta::FromMilliseconds(33);
      constexpr base::TimeDelta kGreetingAnimationFadeInDuration =
          base::TimeDelta::FromMilliseconds(167);
      constexpr base::TimeDelta kGreetingAnimationTranslateUpDuration =
          base::TimeDelta::FromMilliseconds(250);

      // Start animating greeting label.
      greeting_label_->layer()->GetAnimator()->StartTogether(
          {// Animate the transformation.
           CreateLayerAnimationSequence(CreateTransformElement(
               gfx::Transform(), kGreetingAnimationTranslateUpDuration,
               gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
           // Animate the opacity to 100% with delay.
           CreateLayerAnimationSequence(
               ui::LayerAnimationElement::CreatePauseElement(
                   ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                   kGreetingAnimationFadeInDelay),
               CreateOpacityElement(1.f, kGreetingAnimationFadeInDuration))});
    }

    // Set up our pre-animation values.
    footer_->layer()->SetOpacity(0.f);

    // Animate the footer to 100% opacity with delay.
    footer_->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                kFooterEntryAnimationFadeInDelay),
            CreateOpacityElement(1.f, kFooterEntryAnimationFadeInDuration)));
    return;
  }

  query_view_->SetQuery(ash::AssistantNullQuery());

  footer_->SetVisible(true);
  footer_->layer()->SetOpacity(1.f);
  footer_->set_can_process_events_within_subtree(true);
}

void AssistantMainStage::MaybeHideGreetingLabel() {
  if (!IsLayerVisible(greeting_label_))
    return;

  using ash::assistant::util::CreateLayerAnimationSequence;
  using ash::assistant::util::CreateOpacityElement;

  greeting_label_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          CreateOpacityElement(0.f, kGreetingAnimationFadeOutDuration)));
}

void AssistantMainStage::UpdateFooter(bool visible) {
  using ash::assistant::util::CreateLayerAnimationSequence;
  using ash::assistant::util::CreateOpacityElement;
  using ash::assistant::util::CreateTransformElement;
  using ash::assistant::util::StartLayerAnimationSequence;
  using ash::assistant::util::StartLayerAnimationSequencesTogether;

  if (visible == IsLayerVisible(footer_))
    return;

  // Reset visibility to enable animation.
  footer_->SetVisible(true);

  if (visible) {
    // The footer will animate up into position so we need to set an initial
    // offset transformation from which to animate.
    gfx::Transform transform;
    transform.Translate(0, kFooterAnimationTranslationDip);
    footer_->layer()->SetTransform(transform);

    // Animate the entry of the footer.
    StartLayerAnimationSequencesTogether(
        footer_->layer()->GetAnimator(),
        {// Animate the translation with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
                 kFooterAnimationTranslationDelay),
             CreateTransformElement(gfx::Transform(),
                                    kFooterAnimationTranslationDuration,
                                    gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
         // Animate the fade in with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                 kFooterAnimationFadeInDelay),
             CreateOpacityElement(1.f, kFooterAnimationFadeInDuration))},
        // Observer animation start/end events.
        footer_animation_observer_.get());
  } else {
    // Animate the exit of the footer.
    StartLayerAnimationSequence(
        footer_->layer()->GetAnimator(),
        // Animate fade out.
        CreateLayerAnimationSequence(
            CreateOpacityElement(0.f, kFooterAnimationFadeOutDuration)),
        // Observe animation start/end events.
        footer_animation_observer_.get());
  }

  // Set the observer to active so that we'll receive start/end events.
  footer_animation_observer_->SetActive();
}

void AssistantMainStage::OnFooterAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The footer should not process events while animating.
  footer_->set_can_process_events_within_subtree(/*can_process=*/false);
}

bool AssistantMainStage::OnFooterAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  // The footer should only process events when visible.
  const bool visible = IsLayerVisible(footer_);
  footer_->set_can_process_events_within_subtree(visible);
  footer_->SetVisible(visible);

  // Return false so that the observer does not destroy itself.
  return false;
}

}  // namespace app_list
