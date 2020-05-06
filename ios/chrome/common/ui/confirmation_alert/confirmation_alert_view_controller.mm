// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/image_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kConfirmationAlertMoreInfoAccessibilityIdentifier =
    @"kConfirmationAlertMoreInfoAccessibilityIdentifier";
NSString* const kConfirmationAlertTitleAccessibilityIdentifier =
    @"kConfirmationAlertTitleAccessibilityIdentifier";
NSString* const kConfirmationAlertSubtitleAccessibilityIdentifier =
    @"kConfirmationAlertSubtitleAccessibilityIdentifier";
NSString* const kConfirmationAlertPrimaryActionAccessibilityIdentifier =
    @"kConfirmationAlertPrimaryActionAccessibilityIdentifier";
NSString* const kConfirmationAlertBarPrimaryActionAccessibilityIdentifier =
    @"kConfirmationAlertBarPrimaryActionAccessibilityIdentifier";

namespace {

constexpr CGFloat kButtonVerticalInsets = 17;
constexpr CGFloat kPrimaryButtonCornerRadius = 13;
constexpr CGFloat kStackViewSpacing = 8;
constexpr CGFloat kStackViewSpacingAfterIllustration = 27;
constexpr CGFloat kGeneratedImagePadding = 20;
// The multiplier used when in regular horizontal size class.
constexpr CGFloat kSafeAreaMultiplier = 0.8;

}  // namespace

@interface ConfirmationAlertViewController () <UIToolbarDelegate>

// Container view that will wrap the views making up the content.
@property(nonatomic, strong) UIStackView* stackView;

// References to the UI properties that need to be updated when the trait
// collection changes.
@property(nonatomic, strong) UIButton* primaryActionButton;
@property(nonatomic, strong) UIToolbar* topToolbar;
@property(nonatomic, strong) NSArray* regularHeightToolbarItems;
@property(nonatomic, strong) NSArray* compactHeightToolbarItems;
@property(nonatomic, strong) UIImageView* imageView;
// Constraints.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* compactWidthConstraints;
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* regularWidthConstraints;
@property(nonatomic, strong)
    NSLayoutConstraint* regularHeightScrollViewBottomVerticalConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* compactHeightScrollViewBottomVerticalConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* primaryButtonBottomVerticalConstraint;
@end

@implementation ConfirmationAlertViewController

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  self.topToolbar = [self createTopToolbar];
  [self.view addSubview:self.topToolbar];

  self.imageView = [self createImageView];
  UILabel* title = [self createTitleLabel];
  UILabel* subtitle = [self createSubtitleLabel];

  self.stackView = [self
      createStackViewWithArrangedSubviews:@[ self.imageView, title, subtitle ]];

  UIScrollView* scrollView = [self createScrollView];
  [scrollView addSubview:self.stackView];
  [self.view addSubview:scrollView];

  self.view.preservesSuperviewLayoutMargins = YES;
  UILayoutGuide* margins = self.view.layoutMarginsGuide;

  // Toolbar constraints to the top.
  AddSameConstraintsToSides(
      self.topToolbar, self.view.safeAreaLayoutGuide,
      LayoutSides::kTrailing | LayoutSides::kTop | LayoutSides::kLeading);

  // Scroll View constraints to the height of its content. Can be overridden.
  NSLayoutConstraint* heightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.heightAnchor];
  // UILayoutPriorityDefaultHigh is the default priority for content
  // compression. Setting this lower avoids compressing the content of the
  // scroll view.
  heightConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  heightConstraint.active = YES;

  // Scroll View constraint to the vertical center. Can be overridden.
  NSLayoutConstraint* centerYConstraint =
      [scrollView.centerYAnchor constraintEqualToAnchor:margins.centerYAnchor];
  // This needs to be lower than the height constraint, so it's deprioritized.
  // If this breaks, the scroll view is still constrained to the top toolbar and
  // the bottom safe area or button.
  centerYConstraint.priority = heightConstraint.priority - 1;
  centerYConstraint.active = YES;

  // Constraint the content of the scroll view to the size of the stack view.
  // This defines the content area.
  AddSameConstraints(self.stackView, scrollView);

  // Disable horizontal scrolling and constraint the content size to the scroll
  // view size.
  [scrollView.widthAnchor
      constraintEqualToAnchor:scrollView.contentLayoutGuide.widthAnchor]
      .active = YES;

  [scrollView.centerXAnchor constraintEqualToAnchor:margins.centerXAnchor]
      .active = YES;

  // Width Scroll View constraint. It changes based on the size class.
  self.compactWidthConstraints = @[
    [scrollView.widthAnchor constraintEqualToAnchor:margins.widthAnchor],
  ];
  self.regularWidthConstraints = @[
    [scrollView.widthAnchor constraintEqualToAnchor:margins.widthAnchor
                                         multiplier:kSafeAreaMultiplier],
  ];

  // The bottom anchor for the scroll view. It will be updated to the button top
  // anchor if it exists.
  NSLayoutYAxisAnchor* scrollViewBottomAnchor =
      self.view.safeAreaLayoutGuide.bottomAnchor;

  if (self.primaryActionAvailable) {
    UIButton* primaryActionButton = [self createPrimaryActionButton];
    [self.view addSubview:primaryActionButton];

    // Primary Action Button constraints.
    self.primaryButtonBottomVerticalConstraint =
        [primaryActionButton.bottomAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
      [primaryActionButton.leadingAnchor
          constraintEqualToAnchor:scrollView.leadingAnchor],
      [primaryActionButton.trailingAnchor
          constraintEqualToAnchor:scrollView.trailingAnchor],
      self.primaryButtonBottomVerticalConstraint,
    ]];

    scrollViewBottomAnchor = primaryActionButton.topAnchor;
    self.primaryActionButton = primaryActionButton;
  }

  self.regularHeightScrollViewBottomVerticalConstraint =
      [scrollView.bottomAnchor
          constraintLessThanOrEqualToAnchor:scrollViewBottomAnchor];
  self.compactHeightScrollViewBottomVerticalConstraint =
      [scrollView.bottomAnchor
          constraintLessThanOrEqualToAnchor:scrollViewBottomAnchor];

  if (self.alwaysShowImage && self.primaryActionAvailable) {
    // If we always want to show the image, then it means we must hide the
    // button when in compact height mode - meaning we have to constraint the
    // scrollview's bottom to the safeArea's bottom.
    self.compactHeightScrollViewBottomVerticalConstraint =
        [scrollView.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                  .bottomAnchor];
  }

  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.topToolbar.bottomAnchor],
  ]];

  if (!self.imageHasFixedSize) {
    // Constrain the image to the scroll view size and its aspect ratio.
    [self.imageView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [self.imageView
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];
    CGFloat imageAspectRatio =
        self.imageView.image.size.width / self.imageView.image.size.height;

    [NSLayoutConstraint activateConstraints:@[
      [self.imageView.widthAnchor
          constraintEqualToAnchor:self.imageView.heightAnchor
                       multiplier:imageAspectRatio],
    ]];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Update fonts for specific content sizes.
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    self.primaryActionButton.titleLabel.font =
        PreferredFontForTextStyleWithMaxCategory(
            UIFontTextStyleHeadline,
            self.traitCollection.preferredContentSizeCategory,
            UIContentSizeCategoryExtraExtraExtraLarge);
  }

  // Update constraints for different size classes.
  BOOL hasNewHorizontalSizeClass =
      previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass;
  BOOL hasNewVerticalSizeClass = previousTraitCollection.verticalSizeClass !=
                                 self.traitCollection.verticalSizeClass;

  if (hasNewHorizontalSizeClass || hasNewVerticalSizeClass) {
    [self.view setNeedsUpdateConstraints];
  }
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self.view setNeedsUpdateConstraints];
}

- (void)viewLayoutMarginsDidChange {
  [super viewLayoutMarginsDidChange];
  [self.view setNeedsUpdateConstraints];
}

- (void)updateViewConstraints {
  CGFloat marginValue =
      self.view.layoutMargins.left - self.view.safeAreaInsets.left;
  self.primaryButtonBottomVerticalConstraint.constant = -marginValue;
  if (self.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    [NSLayoutConstraint deactivateConstraints:self.regularWidthConstraints];
    [NSLayoutConstraint activateConstraints:self.compactWidthConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:self.compactWidthConstraints];
    [NSLayoutConstraint activateConstraints:self.regularWidthConstraints];
  }

  BOOL isVerticalCompact =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;

  NSLayoutConstraint* oldBottomConstraint;
  NSLayoutConstraint* newBottomConstraint;
  if (isVerticalCompact) {
    oldBottomConstraint = self.regularHeightScrollViewBottomVerticalConstraint;
    newBottomConstraint = self.compactHeightScrollViewBottomVerticalConstraint;
    self.topToolbar.items = self.compactHeightToolbarItems;
  } else {
    oldBottomConstraint = self.compactHeightScrollViewBottomVerticalConstraint;
    newBottomConstraint = self.regularHeightScrollViewBottomVerticalConstraint;
    self.topToolbar.items = self.regularHeightToolbarItems;
  }

  newBottomConstraint.constant = -marginValue;
  [NSLayoutConstraint deactivateConstraints:@[ oldBottomConstraint ]];
  [NSLayoutConstraint activateConstraints:@[ newBottomConstraint ]];

  if (self.alwaysShowImage) {
    // Update the primary action button visibility.
    [self.primaryActionButton setHidden:isVerticalCompact];
  } else {
    [self.imageView setHidden:isVerticalCompact];
  }

  // Allow toolbar to update its height based on new layout.
  [self.topToolbar invalidateIntrinsicContentSize];

  [super updateViewConstraints];
}

- (UIImage*)content {
  UIEdgeInsets padding =
      UIEdgeInsetsMake(kGeneratedImagePadding, kGeneratedImagePadding,
                       kGeneratedImagePadding, kGeneratedImagePadding);
  return ImageFromView(self.stackView, self.view.backgroundColor, padding);
}

#pragma mark - UIToolbarDelegate

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  return UIBarPositionTopAttached;
}

#pragma mark - Private

// Handle taps on the done button.
- (void)didTapDoneButton {
  [self.actionHandler confirmationAlertDone];
}

// Handle taps on the help button.
- (void)didTapHelpButton {
  [self.actionHandler confirmationAlertLearnMoreAction];
}

// Handle taps on the primary action button.
- (void)didTapPrimaryActionButton {
  [self.actionHandler confirmationAlertPrimaryAction];
}

// Helper to create the top toolbar.
- (UIToolbar*)createTopToolbar {
  UIToolbar* topToolbar = [[UIToolbar alloc] init];
  topToolbar.translucent = NO;
  [topToolbar setShadowImage:[[UIImage alloc] init]
          forToolbarPosition:UIBarPositionAny];
  [topToolbar setBarTintColor:[UIColor colorNamed:kBackgroundColor]];
  topToolbar.delegate = self;

  NSMutableArray* regularHeightItems = [[NSMutableArray alloc] init];
  NSMutableArray* compactHeightItems = [[NSMutableArray alloc] init];
  if (self.helpButtonAvailable) {
    UIBarButtonItem* helpButton = [[UIBarButtonItem alloc]
        initWithImage:[UIImage imageNamed:@"confirmation_alert_ic_help"]
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(didTapHelpButton)];
    [regularHeightItems addObject:helpButton];
    [compactHeightItems addObject:helpButton];
    helpButton.accessibilityIdentifier =
        kConfirmationAlertMoreInfoAccessibilityIdentifier;
    // Set the help button as the left button item so it can be used as a
    // popover anchor.
    _helpButton = helpButton;
  }

  if (self.alwaysShowImage && self.primaryActionAvailable) {
    if (self.helpButtonAvailable) {
      // Add margin with help button.
      UIBarButtonItem* fixedSpacer = [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace
                               target:nil
                               action:nil];
      fixedSpacer.width = 15.0f;
      [compactHeightItems addObject:fixedSpacer];
    }

    UIBarButtonItem* primaryActionBarButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:self.primaryActionBarButtonStyle
                             target:self
                             action:@selector(didTapPrimaryActionButton)];
    primaryActionBarButton.accessibilityIdentifier =
        kConfirmationAlertBarPrimaryActionAccessibilityIdentifier;

    // Only shows up in constraint height mode.
    [compactHeightItems addObject:primaryActionBarButton];
  }

  UIBarButtonItem* spacer = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [regularHeightItems addObject:spacer];
  [compactHeightItems addObject:spacer];

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(didTapDoneButton)];
  [regularHeightItems addObject:doneButton];
  [compactHeightItems addObject:doneButton];

  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  self.regularHeightToolbarItems = regularHeightItems;
  self.compactHeightToolbarItems = compactHeightItems;

  return topToolbar;
}

// Helper to create the image view.
- (UIImageView*)createImageView {
  UIImageView* imageView = [[UIImageView alloc] initWithImage:self.image];
  imageView.contentMode = UIViewContentModeScaleAspectFit;

  if (self.imageAccessibilityLabel) {
    imageView.isAccessibilityElement = YES;
    imageView.accessibilityLabel = self.imageAccessibilityLabel;
  }

  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

// Helper to create the title label.
- (UILabel*)createTitleLabel {
  if (!self.titleTextStyle) {
    self.titleTextStyle = UIFontTextStyleTitle1;
  }
  UILabel* title = [[UILabel alloc] init];
  title.numberOfLines = 0;
  UIFontDescriptor* descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:self.titleTextStyle];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics =
      [UIFontMetrics metricsForTextStyle:self.titleTextStyle];
  title.font = [fontMetrics scaledFontForFont:font];
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.text = self.titleString.capitalizedString;
  title.textAlignment = NSTextAlignmentCenter;
  title.translatesAutoresizingMaskIntoConstraints = NO;
  title.adjustsFontForContentSizeCategory = YES;
  title.accessibilityIdentifier =
      kConfirmationAlertTitleAccessibilityIdentifier;
  return title;
}

// Helper to create the subtitle label.
- (UILabel*)createSubtitleLabel {
  UILabel* subtitle = [[UILabel alloc] init];
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitle.numberOfLines = 0;
  subtitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitle.text = self.subtitleString;
  subtitle.textAlignment = NSTextAlignmentCenter;
  subtitle.translatesAutoresizingMaskIntoConstraints = NO;
  subtitle.adjustsFontForContentSizeCategory = YES;
  subtitle.accessibilityIdentifier =
      kConfirmationAlertSubtitleAccessibilityIdentifier;
  return subtitle;
}

// Helper to create the scroll view.
- (UIScrollView*)createScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.alwaysBounceVertical = NO;
  scrollView.showsHorizontalScrollIndicator = NO;
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  return scrollView;
}

// Helper to create the stack view.
- (UIStackView*)createStackViewWithArrangedSubviews:
    (NSArray<UIView*>*)subviews {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:subviews];
  [stackView setCustomSpacing:kStackViewSpacingAfterIllustration
                    afterView:self.imageView];

  if (self.imageHasFixedSize) {
    stackView.alignment = UIStackViewAlignmentCenter;
  } else {
    stackView.alignment = UIStackViewAlignmentFill;
  }

  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = kStackViewSpacing;
  return stackView;
}

// Helper to create the primary action button.
- (UIButton*)createPrimaryActionButton {
  UIButton* primaryActionButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [primaryActionButton addTarget:self
                          action:@selector(didTapPrimaryActionButton)
                forControlEvents:UIControlEventTouchUpInside];
  [primaryActionButton setTitle:self.primaryActionString.capitalizedString
                       forState:UIControlStateNormal];
  primaryActionButton.contentEdgeInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  [primaryActionButton setBackgroundColor:[UIColor colorNamed:kBlueColor]];
  UIColor* titleColor = [UIColor colorNamed:kSolidButtonTextColor];
  [primaryActionButton setTitleColor:titleColor forState:UIControlStateNormal];
  primaryActionButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  primaryActionButton.layer.cornerRadius = kPrimaryButtonCornerRadius;
  primaryActionButton.titleLabel.adjustsFontForContentSizeCategory = NO;
  primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;
  primaryActionButton.accessibilityIdentifier =
      kConfirmationAlertPrimaryActionAccessibilityIdentifier;

#if defined(__IPHONE_13_4)
  if (@available(iOS 13.4, *)) {
    if (self.pointerInteractionEnabled) {
      primaryActionButton.pointerInteractionEnabled = YES;

      primaryActionButton.pointerStyleProvider =
          ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                           __unused UIPointerShape* proposedShape) {
        //
        // The default pointer interaction on this button is awful (It does a
        // lift on the button with an weird mousing highlight effect on just
        // the label). All attempts to correct this for a button wide lift have
        // failed ; no matter what is done in this block the only reasonable
        // effect achievable is a hover with the cursor still visible.
        //
        // It seems that anything larger than roughly the third of the width
        // of an iPad screen causes the framework to cancel the the lift effect
        // and to replace it with a simple hover.
        //
        // This code below should do a lift. Does a lift on a smaller version
        // of the exact same button. But it only achieves a hover.
        //
        return [UIPointerStyle styleWithEffect:proposedEffect shape:nil];
      };
    }
  }
#endif  // defined(__IPHONE_13_4)

  return primaryActionButton;
}

@end
