// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"

#include <map>
#include <memory>

#include "base/logging.h"
#include "base/scoped_observer.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer.h"
#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_view_controller.h"
#import "ios/chrome/browser/ui/overlays/overlay_presentation_context_impl.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Observer that disables fullscreen while overlays are presented.
class OverlayContainerFullscreenDisabler {
 public:
  OverlayContainerFullscreenDisabler(Browser* browser, OverlayModality modality)
      : fullscreen_disabler_(
            FullscreenController::FromBrowserState(browser->GetBrowserState()),
            OverlayPresenter::FromBrowser(browser, modality)) {}
  ~OverlayContainerFullscreenDisabler() = default;
  OverlayContainerFullscreenDisabler(
      OverlayContainerFullscreenDisabler& disabler) = delete;

 private:
  // Helper object that disables fullscreen when overlays are presented.
  class FullscreenDisabler : public OverlayPresenterObserver {
   public:
    FullscreenDisabler(FullscreenController* fullscreen_controller,
                       OverlayPresenter* overlay_presenter)
        : fullscreen_controller_(fullscreen_controller),
          scoped_observer_(this) {
      DCHECK(fullscreen_controller_);
      DCHECK(overlay_presenter);
      scoped_observer_.Add(overlay_presenter);
    }
    ~FullscreenDisabler() override = default;

   private:
    // OverlayPresenterObserver:
    void WillShowOverlay(OverlayPresenter* presenter,
                         OverlayRequest* request) override {
      disabler_ = std::make_unique<AnimatedScopedFullscreenDisabler>(
          fullscreen_controller_);
      disabler_->StartAnimation();
    }

    void DidHideOverlay(OverlayPresenter* presenter,
                        OverlayRequest* request) override {
      disabler_ = nullptr;
    }

    void OverlayPresenterDestroyed(OverlayPresenter* presenter) override {
      scoped_observer_.Remove(presenter);
      disabler_ = nullptr;
    }

    // The FullscreenController being disabled.
    FullscreenController* fullscreen_controller_ = nullptr;
    // The animated disabler.
    std::unique_ptr<AnimatedScopedFullscreenDisabler> disabler_;
    ScopedObserver<OverlayPresenter, OverlayPresenterObserver> scoped_observer_;
  };

  FullscreenDisabler fullscreen_disabler_;
};
}  // namespace

@interface OverlayContainerCoordinator () <
    OverlayContainerViewControllerDelegate> {
  // Helper object that disables fullscreen whenever an overlay is presented.
  std::unique_ptr<OverlayContainerFullscreenDisabler> fullscreen_disabler_;
}
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The presentation context used by OverlayPresenter to drive presentation for
// this container.
@property(nonatomic, readonly)
    OverlayPresentationContextImpl* presentationContext;
// The modality being handled by the container.
@property(nonatomic, assign) OverlayModality modality;
@end

@implementation OverlayContainerCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  modality:(OverlayModality)modality {
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    OverlayPresentationContextImpl::Container::CreateForUserData(browser,
                                                                 browser);
    _presentationContext =
        OverlayPresentationContextImpl::Container::FromUserData(browser)
            ->PresentationContextForModality(modality);
    DCHECK(_presentationContext);
    _modality = modality;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;
  self.started = YES;
  // Create the container view controller and add it to the base view
  // controller.
  OverlayContainerViewController* viewController =
      [[OverlayContainerViewController alloc] init];
  viewController.definesPresentationContext = YES;
  viewController.delegate = self;
  _viewController = viewController;
  UIView* containerView = _viewController.view;
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:containerView];
  AddSameConstraints(containerView, self.baseViewController.view);
  [_viewController didMoveToParentViewController:self.baseViewController];
  self.presentationContext->SetCoordinator(self);
  fullscreen_disabler_ = std::make_unique<OverlayContainerFullscreenDisabler>(
      self.browser, self.modality);
}

- (void)stop {
  if (!self.started)
    return;
  self.started = NO;
  self.presentationContext->SetCoordinator(nil);
  // Remove the container view and reset the view controller.
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
  fullscreen_disabler_ = nullptr;
}

#pragma mark - OverlayContainerViewControllerDelegate

- (void)containerViewController:
            (OverlayContainerViewController*)containerViewController
                didMoveToWindow:(UIWindow*)window {
  self.presentationContext->WindowDidChange();
}

@end
