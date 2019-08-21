// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/layout/animating_layout_manager.h"

#include <utility>

#include "base/auto_reset.h"
#include "chrome/browser/ui/views/layout/interpolating_layout_manager.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

// Manages the animation and various callbacks from the animation system that
// are required to update the layout during animations.
class AnimatingLayoutManager::AnimationDelegate
    : public views::AnimationDelegateViews {
 public:
  explicit AnimationDelegate(AnimatingLayoutManager* layout_manager);
  ~AnimationDelegate() override = default;

  // Returns true after the host view is added to a widget or animation has been
  // enabled by a unit test.
  //
  // Before that, animation is not possible, so all changes to the host view
  // should result in the host view's layout being snapped directly to the
  // target layout.
  bool ready_to_animate() const { return ready_to_animate_; }

  // Pushes animation configuration (tween type, duration) through to the
  // animation itself.
  void UpdateAnimationParameters();

  // Starts the animation.
  void Animate();

  // Cancels and resets the current animation (if any).
  void Reset();

  // If the current layout is not yet ready to animate, transitions into the
  // ready-to-animate state, possibly resetting the current layout and
  // invalidating the host to make sure the layout is up to date.
  void MakeReadyForAnimation();

 private:
  // Observer used to watch for the host view being parented to a widget.
  class ViewWidgetObserver : public views::ViewObserver {
   public:
    explicit ViewWidgetObserver(AnimationDelegate* animation_delegate)
        : animation_delegate_(animation_delegate) {}

    void OnViewAddedToWidget(views::View* observed_view) override {
      animation_delegate_->MakeReadyForAnimation();
    }

    void OnViewIsDeleting(views::View* observed_view) override {
      if (animation_delegate_->scoped_observer_.IsObserving(observed_view))
        animation_delegate_->scoped_observer_.Remove(observed_view);
    }

   private:
    AnimationDelegate* const animation_delegate_;
  };
  friend class Observer;

  // AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  bool ready_to_animate_ = false;
  bool resetting_animation_ = false;
  AnimatingLayoutManager* const target_layout_manager_;
  std::unique_ptr<gfx::SlideAnimation> animation_;
  ViewWidgetObserver view_widget_observer_{this};
  ScopedObserver<views::View, views::ViewObserver> scoped_observer_{
      &view_widget_observer_};
};

AnimatingLayoutManager::AnimationDelegate::AnimationDelegate(
    AnimatingLayoutManager* layout_manager)
    : AnimationDelegateViews(layout_manager->host_view()),
      target_layout_manager_(layout_manager),
      animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  animation_->SetContainer(new gfx::AnimationContainer());
  views::View* const host_view = layout_manager->host_view();
  DCHECK(host_view);
  if (host_view->GetWidget())
    MakeReadyForAnimation();
  else
    scoped_observer_.Add(host_view);
  UpdateAnimationParameters();
}

void AnimatingLayoutManager::AnimationDelegate::UpdateAnimationParameters() {
  animation_->SetTweenType(target_layout_manager_->tween_type());
  animation_->SetSlideDuration(
      target_layout_manager_->animation_duration().InMilliseconds());
}

void AnimatingLayoutManager::AnimationDelegate::Animate() {
  DCHECK(ready_to_animate_);
  Reset();
  animation_->Show();
}

void AnimatingLayoutManager::AnimationDelegate::Reset() {
  if (!ready_to_animate_)
    return;
  base::AutoReset<bool> setter(&resetting_animation_, true);
  animation_->Reset();
}

void AnimatingLayoutManager::AnimationDelegate::MakeReadyForAnimation() {
  if (!ready_to_animate_) {
    target_layout_manager_->ResetLayout();
    target_layout_manager_->InvalidateHost();
    ready_to_animate_ = true;
    if (scoped_observer_.IsObserving(target_layout_manager_->host_view()))
      scoped_observer_.Remove(target_layout_manager_->host_view());
  }
}

void AnimatingLayoutManager::AnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK(animation_.get() == animation);
  target_layout_manager_->AnimateTo(animation->GetCurrentValue());
}

void AnimatingLayoutManager::AnimationDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void AnimatingLayoutManager::AnimationDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  if (resetting_animation_)
    return;
  DCHECK(animation_.get() == animation);
  target_layout_manager_->AnimateTo(1.0);
}

// AnimatingLayoutManager:

AnimatingLayoutManager::AnimatingLayoutManager() {}
AnimatingLayoutManager::~AnimatingLayoutManager() = default;

AnimatingLayoutManager& AnimatingLayoutManager::SetShouldAnimateBounds(
    bool should_animate_bounds) {
  if (should_animate_bounds_ != should_animate_bounds) {
    should_animate_bounds_ = should_animate_bounds;
    ResetLayout();
    InvalidateHost();
  }
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetAnimationDuration(
    base::TimeDelta animation_duration) {
  DCHECK_GE(animation_duration, base::TimeDelta());
  animation_duration_ = animation_duration;
  if (animation_delegate_)
    animation_delegate_->UpdateAnimationParameters();
  return *this;
}

AnimatingLayoutManager& AnimatingLayoutManager::SetTweenType(
    gfx::Tween::Type tween_type) {
  tween_type_ = tween_type;
  if (animation_delegate_)
    animation_delegate_->UpdateAnimationParameters();
  return *this;
}

void AnimatingLayoutManager::ResetLayout() {
  if (animation_delegate_)
    animation_delegate_->Reset();

  if (!target_layout_manager_)
    return;

  const gfx::Size target_size =
      should_animate_bounds_
          ? target_layout_manager_->GetPreferredSize(host_view())
          : host_view()->size();
  target_layout_ = target_layout_manager_->GetProposedLayout(target_size);
  current_layout_ = target_layout_;
  starting_layout_ = current_layout_;
  current_offset_ = 1.0;
  set_cached_layout_size(target_size);
}

void AnimatingLayoutManager::AddObserver(Observer* observer) {
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void AnimatingLayoutManager::RemoveObserver(Observer* observer) {
  if (observers_.HasObserver(observer))
    observers_.RemoveObserver(observer);
}

bool AnimatingLayoutManager::HasObserver(Observer* observer) const {
  return observers_.HasObserver(observer);
}

void AnimatingLayoutManager::SetChildViewIgnoredByLayout(
    views::View* child_view,
    bool ignored) {
  LayoutManagerBase::SetChildViewIgnoredByLayout(child_view, ignored);
  if (target_layout_manager_)
    target_layout_manager_->SetChildViewIgnoredByLayout(child_view, ignored);
}

gfx::Size AnimatingLayoutManager::GetPreferredSize(
    const views::View* host) const {
  if (!target_layout_manager_)
    return gfx::Size();
  return should_animate_bounds_
             ? current_layout_.host_size
             : target_layout_manager_->GetPreferredSize(host);
}

gfx::Size AnimatingLayoutManager::GetMinimumSize(
    const views::View* host) const {
  if (!target_layout_manager_)
    return gfx::Size();
  // TODO(dfried): consider cases where the minimum size might not be just the
  // minimum size of the embedded layout.
  return target_layout_manager_->GetMinimumSize(host);
}

int AnimatingLayoutManager::GetPreferredHeightForWidth(const views::View* host,
                                                       int width) const {
  if (!target_layout_manager_)
    return 0;
  // TODO(dfried): revisit this computation.
  return should_animate_bounds_
             ? current_layout_.host_size.height()
             : target_layout_manager_->GetPreferredHeightForWidth(host, width);
}

void AnimatingLayoutManager::Layout(views::View* host) {
  DCHECK_EQ(host_view(), host);
  // Changing the size of a view directly will lead to a layout call rather
  // than an invalidation. This should reset the layout (but see the note in
  // RecalculateTarget() below).
  if (!should_animate_bounds_ &&
      (!cached_layout_size() || host->size() != *cached_layout_size())) {
    ResetLayout();
  }
  ApplyLayout(current_layout_);

  // Send animating stopped events on layout so the current layout during the
  // event represents the final state instead of an intermediate state.
  if (is_animating_ && current_offset_ == 1.0) {
    is_animating_ = false;
    NotifyIsAnimatingChanged();
  }
}

void AnimatingLayoutManager::InvalidateLayout() {
  if (suspend_recalculation_)
    return;
  if (target_layout_manager_)
    target_layout_manager_->InvalidateLayout();
  RecalculateTarget();
}

void AnimatingLayoutManager::Installed(views::View* host) {
  LayoutManagerBase::Installed(host);
  DCHECK(!animation_delegate_);
  animation_delegate_ = std::make_unique<AnimationDelegate>(this);
  if (target_layout_manager_)
    target_layout_manager_->Installed(host);
}

gfx::AnimationContainer*
AnimatingLayoutManager::GetAnimationContainerForTesting() {
  DCHECK(animation_delegate_);
  animation_delegate_->MakeReadyForAnimation();
  DCHECK(animation_delegate_->ready_to_animate());
  return animation_delegate_->container();
}

void AnimatingLayoutManager::EnableAnimationForTesting() {
  DCHECK(animation_delegate_);
  animation_delegate_->MakeReadyForAnimation();
  DCHECK(animation_delegate_->ready_to_animate());
}

views::LayoutManagerBase::ProposedLayout
AnimatingLayoutManager::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  // This class directly overrides Layout() so GetProposedLayout() and
  // CalculateProposedLayout() are not called.
  NOTREACHED();
  return ProposedLayout();
}

void AnimatingLayoutManager::ViewAdded(views::View* host, views::View* view) {
  {
    base::AutoReset<bool> setter(&suspend_recalculation_, true);
    LayoutManagerBase::ViewAdded(host, view);
    if (target_layout_manager_)
      target_layout_manager_->ViewAdded(host, view);
  }
  if (RecalculateTarget())
    InvalidateHost();
}

void AnimatingLayoutManager::ViewRemoved(views::View* host, views::View* view) {
  {
    base::AutoReset<bool> setter(&suspend_recalculation_, true);
    LayoutManagerBase::ViewRemoved(host, view);
    if (target_layout_manager_)
      target_layout_manager_->ViewRemoved(host, view);
  }
  if (RecalculateTarget())
    InvalidateHost();
}

void AnimatingLayoutManager::ViewVisibilitySet(views::View* host,
                                               views::View* view,
                                               bool visible) {
  {
    base::AutoReset<bool> setter(&suspend_recalculation_, true);
    LayoutManagerBase::ViewVisibilitySet(host, view, visible);
    if (target_layout_manager_)
      target_layout_manager_->ViewVisibilitySet(host, view, visible);
  }
  if (RecalculateTarget())
    InvalidateHost();
}

void AnimatingLayoutManager::SetTargetLayoutManagerImpl(
    std::unique_ptr<LayoutManagerBase> target_layout_manager) {
  DCHECK(!target_layout_manager_);
  DCHECK(target_layout_manager);
  target_layout_manager_ = std::move(target_layout_manager);
  SyncStateTo(target_layout_manager_.get());
  ResetLayout();
  InvalidateHost();
}

void AnimatingLayoutManager::InvalidateHost() {
  base::AutoReset<bool> setter(&suspend_recalculation_, true);
  host_view()->InvalidateLayout();
}

bool AnimatingLayoutManager::RecalculateTarget() {
  constexpr double kResetAnimationThreshold = 0.8;

  if (!target_layout_manager_)
    return false;

  if (!cached_layout_size() || !animation_delegate_ ||
      !animation_delegate_->ready_to_animate()) {
    ResetLayout();
    return true;
  }

  const gfx::Size target_size =
      should_animate_bounds_
          ? target_layout_manager_->GetPreferredSize(host_view())
          : host_view()->size();

  // For layouts that are confined to available space, changing the available
  // space causes a fresh layout, not an animation.
  // TODO(dfried): define a way for views to animate into and out of empty space
  // as adjacent child views appear/disappear. This will be useful in animating
  // tab titles, which currently slide over when the favicon disappears.
  if (!should_animate_bounds_ && *cached_layout_size() != target_size) {
    ResetLayout();
    return true;
  }

  set_cached_layout_size(target_size);

  // If there has been no appreciable change in layout, there's no reason to
  // start or update an animation.
  const ProposedLayout proposed_layout =
      target_layout_manager_->GetProposedLayout(target_size);
  if (target_layout_ == proposed_layout)
    return false;

  starting_layout_ = current_layout_;
  target_layout_ = proposed_layout;
  if (current_offset_ > kResetAnimationThreshold) {
    starting_offset_ = 0.0;
    current_offset_ = 0.0;
    animation_delegate_->Animate();
    if (!is_animating_) {
      is_animating_ = true;
      NotifyIsAnimatingChanged();
    }
  } else {
    starting_offset_ = current_offset_;
  }
  // This drops out any child view elements that don't exist in the target
  // layout.
  current_layout_ = InterpolatingLayoutManager::Interpolate(
      0.0, starting_layout_, target_layout_);
  return true;
}

void AnimatingLayoutManager::AnimateTo(double value) {
  DCHECK_GE(value, 0.0);
  DCHECK_LE(value, 1.0);
  DCHECK_GE(value, starting_offset_);
  DCHECK_GE(value, current_offset_);
  if (current_offset_ == value)
    return;
  current_offset_ = value;
  double percent =
      (current_offset_ - starting_offset_) / (1.0 - starting_offset_);
  current_layout_ = InterpolatingLayoutManager::Interpolate(
      percent, starting_layout_, target_layout_);
  InvalidateHost();
}

void AnimatingLayoutManager::NotifyIsAnimatingChanged() {
  for (auto& observer : observers_)
    observer.OnLayoutIsAnimatingChanged(this, is_animating());
}
