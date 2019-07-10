// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Banner View constants.
const CGFloat kBannerViewCornerRadius = 13.0;
const CGFloat kBannerViewYShadowOffset = 3.0;
const CGFloat kBannerViewShadowRadius = 9.0;
const CGFloat kBannerViewShadowOpacity = 0.23;

// Banner View selected constants.
const CGFloat kTappedBannerViewScale = 0.98;
const CGFloat kSelectedBannerViewScale = 1.02;
const CGFloat kSelectBannerAnimationDurationInSeconds = 0.2;
const CGFloat kTappedBannerAnimationDurationInSeconds = 0.05;
const CGFloat kSelectedBannerViewYShadowOffset = 8.0;

// Bottom Grip constants.
const CGFloat kBottomGripCornerRadius = 0.2;
const CGFloat kBottomGripWidth = 44.0;
const CGFloat kBottomGripHeight = 3.0;
const CGFloat kBottomGripBottomPadding = 4.0;
const int kBottomGripBackgroundColor = 0xD8D8D8;

// Labels constants.
const int kTitleLabelColor = 0x202124;
const int kSubTitleLabelColor = 0x7F868C;

// Button constants.
const CGFloat kButtonWidth = 100.0;
const CGFloat kButtonSeparatorWidth = 1.0;
const int kButtonSeparatorColor = 0xF1F3F4;
const CGFloat kButtonMaxFontSize = 45;

// Container Stack constants.
const CGFloat kContainerStackSpacing = 18.0;
const CGFloat kContainerStackVerticalPadding = 18.0;

// Icon constants.
const CGFloat kIconWidth = 25.0;

// Gesture constants.
const CGFloat kChangeInPositionForTransition = 100.0;
const CGFloat kChangeInPositionForDismissal = -15.0;
const CGFloat kLongPressTimeDurationInSeconds = 0.4;
}  // namespace

@interface InfobarBannerViewController ()

// The original position of this InfobarVC view in the parent's view coordinate
// system.
@property(nonatomic, assign) CGPoint originalCenter;
// The starting point of the LongPressGesture, used to calculate the gesture
// translation.
@property(nonatomic, assign) CGPoint startingTouch;
// Delegate to handle this InfobarVC actions.
@property(nonatomic, weak) id<InfobarBannerDelegate> delegate;
// YES if the user is interacting with the view via a touch gesture.
@property(nonatomic, assign) BOOL touchInProgress;
// YES if the view should be dismissed after any touch gesture has ended.
@property(nonatomic, assign) BOOL shouldDismissAfterTouchesEnded;
// UIButton with title |self.buttonText|, which triggers the Infobar action.
@property(nonatomic, strong) UIButton* infobarButton;
// UILabel displaying |self.titleText|.
@property(nonatomic, strong) UILabel* titleLabel;
// UILabel displaying |self.subTitleText|.
@property(nonatomic, strong) UILabel* subTitleLabel;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;
// The NSTimeInterval in which the Banner appeared on screen.
@property(nonatomic, assign) NSTimeInterval bannerAppearedTime;
// YES if the banner on screen time metric has already been recorded for this
// banner.
@property(nonatomic, assign) BOOL bannerOnScreenTimeWasRecorded;

@end

@implementation InfobarBannerViewController
@synthesize interactionDelegate = _interactionDelegate;

- (instancetype)initWithDelegate:(id<InfobarBannerDelegate>)delegate
                            type:(InfobarType)infobarType {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _delegate = delegate;
    _metricsRecorder =
        [[InfobarMetricsRecorder alloc] initWithType:infobarType];
  }
  return self;
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  // BannerView setup.
  self.view.backgroundColor = [UIColor whiteColor];
  self.view.layer.cornerRadius = kBannerViewCornerRadius;
  [self.view.layer setShadowColor:[UIColor blackColor].CGColor];
  [self.view.layer setShadowOffset:CGSizeMake(0.0, kBannerViewYShadowOffset)];
  [self.view.layer setShadowRadius:kBannerViewShadowRadius];
  [self.view.layer setShadowOpacity:kBannerViewShadowOpacity];
  self.view.accessibilityIdentifier = kInfobarBannerViewIdentifier;
  self.view.isAccessibilityElement = YES;
  self.view.accessibilityLabel = [self accessibilityLabel];
  self.view.accessibilityCustomActions = [self accessibilityActions];

  // Bottom Grip setup.
  UIView* bottomGrip = [[UIView alloc] init];
  bottomGrip.backgroundColor = UIColorFromRGB(kBottomGripBackgroundColor);
  bottomGrip.layer.cornerRadius = kBottomGripCornerRadius;
  bottomGrip.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:bottomGrip];

  // Icon setup.
  self.iconImage = [self.iconImage
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIImageView* iconImageView =
      [[UIImageView alloc] initWithImage:self.iconImage];
  iconImageView.contentMode = UIViewContentModeScaleAspectFit;

  // Labels setup.
  self.titleLabel = [[UILabel alloc] init];
  self.titleLabel.text = self.titleText;
  self.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  self.titleLabel.adjustsFontForContentSizeCategory = YES;
  self.titleLabel.textColor = UIColorFromRGB(kTitleLabelColor);
  self.titleLabel.numberOfLines = 0;
  self.titleLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;

  self.subTitleLabel = [[UILabel alloc] init];
  self.subTitleLabel.text = self.subTitleText;
  self.subTitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  self.subTitleLabel.adjustsFontForContentSizeCategory = YES;
  self.subTitleLabel.textColor = UIColorFromRGB(kSubTitleLabelColor);
  self.subTitleLabel.numberOfLines = 0;
  // If |self.subTitleText| hasn't been set or is empty, hide the label to keep
  // the title label centered in the Y axis.
  self.subTitleLabel.hidden = ![self.subTitleText length];

  UIStackView* labelsStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.titleLabel, self.subTitleLabel ]];
  labelsStackView.axis = UILayoutConstraintAxisVertical;

  // Button setup.
  self.infobarButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [self.infobarButton setTitle:self.buttonText forState:UIControlStateNormal];
  self.infobarButton.titleLabel.font = [[UIFontMetrics defaultMetrics]
      scaledFontForFont:[UIFont
                            preferredFontForTextStyle:UIFontTextStyleHeadline]
       maximumPointSize:kButtonMaxFontSize];
  [self.infobarButton addTarget:self
                         action:@selector(bannerInfobarButtonWasPressed:)
               forControlEvents:UIControlEventTouchUpInside];
  self.infobarButton.accessibilityIdentifier =
      kInfobarBannerAcceptButtonIdentifier;

  UIView* buttonSeparator = [[UIView alloc] init];
  buttonSeparator.translatesAutoresizingMaskIntoConstraints = NO;
  buttonSeparator.backgroundColor = UIColorFromRGB(kButtonSeparatorColor);
  [self.infobarButton addSubview:buttonSeparator];

  // Container Stack setup.
  UIStackView* containerStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    iconImageView, labelsStackView, self.infobarButton
  ]];
  containerStack.axis = UILayoutConstraintAxisHorizontal;
  containerStack.spacing = kContainerStackSpacing;
  containerStack.distribution = UIStackViewDistributionFill;
  containerStack.alignment = UIStackViewAlignmentFill;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.layoutMarginsRelativeArrangement = YES;
  containerStack.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kContainerStackVerticalPadding, 0, kContainerStackVerticalPadding, 0);
  containerStack.insetsLayoutMarginsFromSafeArea = NO;
  [self.view addSubview:containerStack];

  // Constraints setup.
  [NSLayoutConstraint activateConstraints:@[
    // Container Stack.
    [containerStack.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kContainerStackSpacing],
    [containerStack.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [containerStack.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [containerStack.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    // Icon.
    [iconImageView.widthAnchor constraintEqualToConstant:kIconWidth],
    // Button.
    [self.infobarButton.widthAnchor constraintEqualToConstant:kButtonWidth],
    [buttonSeparator.widthAnchor
        constraintEqualToConstant:kButtonSeparatorWidth],
    [buttonSeparator.leadingAnchor
        constraintEqualToAnchor:self.infobarButton.leadingAnchor],
    [buttonSeparator.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [buttonSeparator.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    // Bottom Grip.
    [bottomGrip.widthAnchor constraintEqualToConstant:kBottomGripWidth],
    [bottomGrip.heightAnchor constraintEqualToConstant:kBottomGripHeight],
    [bottomGrip.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [bottomGrip.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor
                                            constant:-kBottomGripBottomPadding],
  ]];

  // Gestures setup.
  UIPanGestureRecognizer* panGestureRecognizer =
      [[UIPanGestureRecognizer alloc] init];
  [panGestureRecognizer addTarget:self action:@selector(handleGestures:)];
  [panGestureRecognizer setMaximumNumberOfTouches:1];
  [self.view addGestureRecognizer:panGestureRecognizer];

  UILongPressGestureRecognizer* longPressGestureRecognizer =
      [[UILongPressGestureRecognizer alloc] init];
  [longPressGestureRecognizer addTarget:self action:@selector(handleGestures:)];
  longPressGestureRecognizer.minimumPressDuration =
      kLongPressTimeDurationInSeconds;
  [self.view addGestureRecognizer:longPressGestureRecognizer];

  UITapGestureRecognizer* tapGestureRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(animateBannerTappedAndPresentModal)];
  [self.view addGestureRecognizer:tapGestureRecognizer];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Presented];
  self.bannerAppearedTime = [NSDate timeIntervalSinceReferenceDate];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  // Call recordBannerOnScreenTime on viewWillDisappear since viewDidDisappear
  // is called after the dismissal animation has occured.
  [self recordBannerOnScreenTime];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Dismissed];
  [self.delegate infobarBannerWasDismissed];
  [super viewDidDisappear:animated];
}

#pragma mark - Public Methods

- (void)dismissWhenInteractionIsFinished {
  if (!self.touchInProgress) {
    [self.metricsRecorder
        recordBannerDismissType:MobileMessagesBannerDismissType::TimedOut];
    [self.delegate dismissInfobarBanner:self animated:YES completion:nil];
  }
  self.shouldDismissAfterTouchesEnded = YES;
}

#pragma mark - Private Methods

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  [self.interactionDelegate infobarBannerStartedInteraction];
  [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Accepted];
  [self.delegate bannerInfobarButtonWasPressed:sender];
}

- (void)handleGestures:(UILongPressGestureRecognizer*)gesture {
  CGPoint touchLocation = [gesture locationInView:self.view];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    [self.interactionDelegate infobarBannerStartedInteraction];
    [self.metricsRecorder recordBannerEvent:MobileMessagesBannerEvent::Handled];
    self.originalCenter = self.view.center;
    self.touchInProgress = YES;
    self.startingTouch = touchLocation;
    [self animateBannerToScaleUpState];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    self.view.center =
        CGPointMake(self.view.center.x, self.view.center.y + touchLocation.y -
                                            self.startingTouch.y);
    // If dragged down by more than kChangeInPositionForTransition, present
    // the InfobarModal.
    BOOL dragDownExceededThreshold =
        (self.view.center.y - self.originalCenter.y >
         kChangeInPositionForTransition);
    if (dragDownExceededThreshold) {
      base::RecordAction(
          base::UserMetricsAction("MobileMessagesBannerDraggedDown"));
      [self.metricsRecorder
          recordBannerDismissType:MobileMessagesBannerDismissType::
                                      ExpandedToModal];
      [self recordBannerOnScreenTime];
      [self.delegate presentInfobarModalFromBanner];
      // Since the modal has now been presented prevent any external dismissal.
      self.shouldDismissAfterTouchesEnded = NO;
      // Cancel the gesture since the modal has now been presented.
      gesture.state = UIGestureRecognizerStateCancelled;
      return;
    }
  }

  if (gesture.state == UIGestureRecognizerStateEnded) {
    [self animateBannerToOriginalStateWithDuration:
              kSelectBannerAnimationDurationInSeconds
                                        completion:nil];
    // If dragged up by more than kChangeInPositionForDismissal at the time
    // the gesture ended, OR |self.shouldDismissAfterTouchesEnded| is YES.
    // Dismiss the banner.
    BOOL dragUpExceededThreshold = (self.view.center.y - self.originalCenter.y -
                                        kChangeInPositionForDismissal <
                                    0);
    if (dragUpExceededThreshold || self.shouldDismissAfterTouchesEnded) {
      if (dragUpExceededThreshold) {
        [self.metricsRecorder
            recordBannerDismissType:MobileMessagesBannerDismissType::SwipedUp];
      } else {
        [self.metricsRecorder
            recordBannerDismissType:MobileMessagesBannerDismissType::TimedOut];
      }
      [self.delegate dismissInfobarBanner:self animated:YES completion:nil];
    } else {
      [self.metricsRecorder
          recordBannerEvent:MobileMessagesBannerEvent::ReturnedToOrigin];
      [self animateBannerToOriginalPosition];
    }
  }

  if (gesture.state == UIGestureRecognizerStateCancelled) {
    // Reset the superview transform so its frame is valid again.
    self.view.superview.transform = CGAffineTransformIdentity;
  }
}

// Animate the Banner being selected by scaling it up.
- (void)animateBannerToScaleUpState {
  [UIView animateWithDuration:kSelectBannerAnimationDurationInSeconds
                   animations:^{
                     self.view.superview.transform = CGAffineTransformMakeScale(
                         kSelectedBannerViewScale, kSelectedBannerViewScale);
                     [self.view.layer
                         setShadowOffset:CGSizeMake(
                                             0.0,
                                             kSelectedBannerViewYShadowOffset)];
                   }
                   completion:nil];
}

// Animate the Banner back to its original size and styling.
- (void)animateBannerToOriginalStateWithDuration:(NSTimeInterval)duration
                                      completion:(ProceduralBlock)completion {
  [UIView animateWithDuration:duration
      animations:^{
        self.view.superview.transform = CGAffineTransformIdentity;
        [self.view.layer
            setShadowOffset:CGSizeMake(0.0, kBannerViewYShadowOffset)];
      }
      completion:^(BOOL finished) {
        if (completion)
          completion();
      }];
}

// Animate the banner back to its original position.
- (void)animateBannerToOriginalPosition {
  [UIView animateWithDuration:kSelectBannerAnimationDurationInSeconds
                   animations:^{
                     self.view.center = self.originalCenter;
                   }
                   completion:nil];
}

// Animate the Banner being tapped by scaling it down and then to its original
// state. After the animation it presentd the Infobar Modal.
- (void)animateBannerTappedAndPresentModal {
  [self.interactionDelegate infobarBannerStartedInteraction];
  // TODO(crbug.com/961343): Interrupt this animation in case the Banner needs
  // to be dismissed mid tap (Currently it will be dismmissed after the
  // animation).
  [UIView animateWithDuration:kTappedBannerAnimationDurationInSeconds
      animations:^{
        self.view.superview.transform = CGAffineTransformMakeScale(
            kTappedBannerViewScale, kTappedBannerViewScale);
        [self.view.layer
            setShadowOffset:CGSizeMake(0.0, kSelectedBannerViewYShadowOffset)];
      }
      completion:^(BOOL finished) {
        [self
            animateBannerToOriginalStateWithDuration:
                kTappedBannerAnimationDurationInSeconds
                                          completion:^{
                                            [self presentInfobarModalAfterTap];
                                          }];
      }];
}

- (void)presentInfobarModalAfterTap {
  base::RecordAction(base::UserMetricsAction("MobileMessagesBannerTapped"));
  [self.metricsRecorder
      recordBannerDismissType:MobileMessagesBannerDismissType::TappedToModal];
  [self recordBannerOnScreenTime];
  [self.delegate presentInfobarModalFromBanner];
}

// Records the banner on screen time. This method should be called as soon as
// its know that the banner will not be visible. This might happen before
// viewWillDissapear since presenting a Modal makes the banner invisible but
// doesn't call viewWillDissapear.
- (void)recordBannerOnScreenTime {
  if (!self.bannerOnScreenTimeWasRecorded) {
    double duration =
        [NSDate timeIntervalSinceReferenceDate] - self.bannerAppearedTime;
    [self.metricsRecorder recordBannerOnScreenDuration:duration];
    self.bannerOnScreenTimeWasRecorded = YES;
  }
}

#pragma mark - Accessibility

- (NSArray*)accessibilityActions {
  UIAccessibilityCustomAction* acceptAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:self.buttonText
                target:self
              selector:@selector(acceptInfobar)];

  UIAccessibilityCustomAction* dismissAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_INFOBAR_BANNER_DISMISS_HINT)
                target:self
              selector:@selector(dismiss)];

  UIAccessibilityCustomAction* modalAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_INFOBAR_BANNER_OPTIONS_HINT)
                target:self
              selector:@selector(triggerInfobarModal)];

  return @[ acceptAction, dismissAction, modalAction ];
}

// A11y Custom actions selectors need to return a BOOL.
- (BOOL)acceptInfobar {
  [self bannerInfobarButtonWasPressed:nil];
  return NO;
}

- (BOOL)triggerInfobarModal {
  [self presentInfobarModalAfterTap];
  return NO;
}

- (BOOL)dismiss {
  [self.delegate dismissInfobarBanner:self animated:YES completion:nil];
  return NO;
}

- (NSString*)accessibilityLabel {
  if ([self.optionalAccessibilityLabel length])
    return self.optionalAccessibilityLabel;
  NSString* accessibilityLabel = self.titleText;
  if ([self.subTitleText length]) {
    accessibilityLabel =
        [NSString stringWithFormat:@"%@,%@", self.titleText, self.subTitleText];
  }
  return accessibilityLabel;
}

@end
