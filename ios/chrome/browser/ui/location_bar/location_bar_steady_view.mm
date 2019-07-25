// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Length of the trailing button side.
const CGFloat kButtonSize = 24;

// Space between the location icon and the location label.
const CGFloat kLocationImageToLabelSpacing = -4.0;
// Minimal horizontal padding between the leading edge of the location bar and
// the content of the location bar.
const CGFloat kLocationBarLeadingPadding = 5.0;
// Trailing space between the trailing button and the trailing edge of the
// location bar.
const CGFloat kButtonTrailingSpacing = 10;

}  // namespace

@interface LocationBarSteadyView ()

// The image view displaying the current location icon (i.e. http[s] status).
@property(nonatomic, strong) UIImageView* locationIconImageView;

// The view containing the location label, and (sometimes) the location image
// view.
@property(nonatomic, strong) UIView* locationContainerView;

// Constraints to hide the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* hideLocationImageConstraints;

// Constraints to show the location image view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* showLocationImageConstraints;

// Elements to surface in accessibility.
@property(nonatomic, strong) NSMutableArray* accessibleElements;

@end

#pragma mark - LocationBarSteadyViewColorScheme

@implementation LocationBarSteadyViewColorScheme

+ (instancetype)standardScheme {
  LocationBarSteadyViewColorScheme* scheme =
      [[LocationBarSteadyViewColorScheme alloc] init];

  scheme.fontColor = [UIColor colorNamed:kTextPrimaryColor];
  scheme.placeholderColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  scheme.trailingButtonColor = [UIColor colorNamed:@"tab_toolbar_button_color"];

  return scheme;
}

+ (instancetype)incognitoScheme {
  LocationBarSteadyViewColorScheme* scheme =
      [[LocationBarSteadyViewColorScheme alloc] init];

  // In iOS 12, the overridePreferredInterfaceStyle API is unavailable, so
  // incognito colors need to be set specifically.
  // TODO(crbug.com/981889): Clean up after iOS 12 support is dropped.
  scheme.fontColor = [UIColor colorNamed:kTextPrimaryDarkColor];
  scheme.placeholderColor = [UIColor colorNamed:kTextfieldPlaceholderDarkColor];
  scheme.trailingButtonColor =
      [UIColor colorNamed:@"tab_toolbar_button_color_incognito"];

  return scheme;
}

@end

#pragma mark - LocationBarSteadyButton

// Buttons with a darker background in highlighted state.
@interface LocationBarSteadyButton : UIButton
@end

@implementation LocationBarSteadyButton

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = self.bounds.size.height / 2.0;
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  CGFloat duration = highlighted ? 0.1 : 0.2;
  [UIView animateWithDuration:duration
                        delay:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     CGFloat alpha = highlighted ? 0.07 : 0;
                     self.backgroundColor =
                         [UIColor colorWithWhite:0 alpha:alpha];
                   }
                   completion:nil];
}

@end

#pragma mark - LocationBarSteadyView

@implementation LocationBarSteadyView

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _locationLabel = [[UILabel alloc] init];
    _locationIconImageView = [[UIImageView alloc] init];
    _locationIconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [_locationIconImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    SetA11yLabelAndUiAutomationName(
        _locationIconImageView,
        IDS_IOS_PAGE_INFO_SECURITY_BUTTON_ACCESSIBILITY_LABEL,
        @"Page Security Info");
    _locationIconImageView.isAccessibilityElement = YES;

    // Setup trailing button.
    _trailingButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
    _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;

    // Setup label.
    _locationLabel.lineBreakMode = NSLineBreakByTruncatingHead;
    _locationLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_locationLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:UILayoutConstraintAxisVertical];
    _locationLabel.font = [self locationLabelFont];

    // Container for location label and icon.
    _locationContainerView = [[UIView alloc] init];
    _locationContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    _locationContainerView.userInteractionEnabled = NO;
    [_locationContainerView addSubview:_locationIconImageView];
    [_locationContainerView addSubview:_locationLabel];

    _showLocationImageConstraints = @[
      [_locationContainerView.leadingAnchor
          constraintEqualToAnchor:_locationIconImageView.leadingAnchor],
      [_locationIconImageView.trailingAnchor
          constraintEqualToAnchor:_locationLabel.leadingAnchor
                         constant:kLocationImageToLabelSpacing],
      [_locationLabel.trailingAnchor
          constraintEqualToAnchor:_locationContainerView.trailingAnchor],
      [_locationIconImageView.centerYAnchor
          constraintEqualToAnchor:_locationContainerView.centerYAnchor],
    ];

    _hideLocationImageConstraints = @[
      [_locationContainerView.leadingAnchor
          constraintEqualToAnchor:_locationLabel.leadingAnchor],
      [_locationLabel.trailingAnchor
          constraintEqualToAnchor:_locationContainerView.trailingAnchor],
    ];

    [NSLayoutConstraint activateConstraints:_showLocationImageConstraints];

    _locationButton = [[LocationBarSteadyButton alloc] init];
    _locationButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_locationButton addSubview:_trailingButton];
    [_locationButton addSubview:_locationContainerView];

    [self addSubview:_locationButton];

    ApplyVisualConstraints(
        @[ @"V:|[label]|", @"V:|[container]|" ],
        @{@"label" : _locationLabel, @"container" : _locationContainerView});

    AddSameConstraints(self, _locationButton);

    // Make the label gravitate towards the center of the view.
    NSLayoutConstraint* centerX = [_locationContainerView.centerXAnchor
        constraintEqualToAnchor:self.centerXAnchor];
    centerX.priority = UILayoutPriorityDefaultHigh;

    // Setup and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_trailingButton.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [_locationContainerView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [_trailingButton.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:_locationContainerView
                                                   .trailingAnchor],
      [_trailingButton.widthAnchor constraintEqualToConstant:kButtonSize],
      [_trailingButton.heightAnchor constraintEqualToConstant:kButtonSize],
      [self.trailingButton.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kButtonTrailingSpacing],
      centerX,
    ]];

    if (IsInfobarUIRebootEnabled()) {
      // Setup leading button.
      _leadingButton = [InfobarBadgeButton buttonWithType:UIButtonTypeSystem];
      _leadingButton.translatesAutoresizingMaskIntoConstraints = NO;
      [_locationButton addSubview:_leadingButton];

      // Setup and activate the leading button constraints.
      [NSLayoutConstraint activateConstraints:@[
        [_leadingButton.widthAnchor
            constraintEqualToAnchor:_leadingButton.heightAnchor],
        [_leadingButton.topAnchor constraintEqualToAnchor:self.topAnchor],
        [_leadingButton.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [_leadingButton.leadingAnchor
            constraintEqualToAnchor:self.leadingAnchor],
        [_leadingButton.trailingAnchor
            constraintLessThanOrEqualToAnchor:_locationContainerView
                                                  .leadingAnchor],
        [_leadingButton.centerYAnchor
            constraintEqualToAnchor:self.centerYAnchor],
      ]];
    } else {
      // Since there is no leading button, |locationContainerView|'s
      // leadingAnchor will be pinned to |self|.
      [NSLayoutConstraint activateConstraints:@[
        [_locationContainerView.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                        constant:kLocationBarLeadingPadding],
      ]];
    }
  }

  // Setup accessibility.
  _trailingButton.isAccessibilityElement = YES;
  if (IsInfobarUIRebootEnabled()) {
    _leadingButton.isAccessibilityElement = YES;
    _leadingButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PASSWORD_HINT);
  }
  _locationButton.isAccessibilityElement = YES;
  _locationButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_LOCATION);

  _accessibleElements = [[NSMutableArray alloc] init];
  [_accessibleElements addObject:_locationButton];
  [_accessibleElements addObject:_trailingButton];

  // These two elements must remain accessible for egtests, but will not be
  // included in accessibility navigation as they are not added to the
  // accessibleElements array.
  _locationIconImageView.isAccessibilityElement = YES;
  _locationLabel.isAccessibilityElement = YES;

  [self updateAccessibility];

  return self;
}

- (void)setColorScheme:(LocationBarSteadyViewColorScheme*)colorScheme {
  _colorScheme = colorScheme;
  self.trailingButton.tintColor = self.colorScheme.trailingButtonColor;
  // The text color is set in -setLocationLabelText: and
  // -setLocationLabelPlaceholderText: because the two text styles have
  // different colors. The icon should be the same color as the text, but it
  // only appears with the regular label, so its color can be set here.
  self.locationIconImageView.tintColor = self.colorScheme.fontColor;
}

- (void)setLocationImage:(UIImage*)locationImage {
  BOOL hadImage = self.locationIconImageView.image != nil;
  BOOL hasImage = locationImage != nil;
  self.locationIconImageView.image = locationImage;
  if (hadImage == hasImage) {
    return;
  }

  if (hasImage) {
    [self.locationContainerView addSubview:self.locationIconImageView];
    [NSLayoutConstraint
        deactivateConstraints:self.hideLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.showLocationImageConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:self.showLocationImageConstraints];
    [NSLayoutConstraint activateConstraints:self.hideLocationImageConstraints];
    [self.locationIconImageView removeFromSuperview];
  }
}

- (void)setLocationLabelText:(NSString*)string {
  if ([self.locationLabel.text isEqualToString:string]) {
    return;
  }
  self.locationLabel.textColor = self.colorScheme.fontColor;
  self.locationLabel.text = string;
  [self updateAccessibility];
}

- (void)setLocationLabelPlaceholderText:(NSString*)string {
  self.locationLabel.textColor = self.colorScheme.placeholderColor;
  self.locationLabel.text = string;
}

- (void)setSecurityLevelAccessibilityString:(NSString*)string {
  if ([_securityLevelAccessibilityString isEqualToString:string]) {
    return;
  }
  _securityLevelAccessibilityString = [string copy];
  [self updateAccessibility];
}

- (void)displayBadge:(BOOL)display animated:(BOOL)animated {
  if (display) {
    // Adding InfobarBadge button as an accessibility element behind location
    // label. Thus, there should be at least one object alreading in
    // |accessibleElements|.
    DCHECK([self.accessibleElements count] > 0);
    [self.accessibleElements insertObject:self.leadingButton atIndex:1];
  } else {
    [self.accessibleElements removeObject:self.leadingButton];
  }
  [self.leadingButton displayBadge:display animated:animated];
}

- (void)enableTrailingButton:(BOOL)enabled {
  self.trailingButton.enabled = enabled;
  [self updateAccessibility];
}

#pragma mark - UIResponder

// This is needed for UIMenu
- (BOOL)canBecomeFirstResponder {
  return true;
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    self.locationLabel.font = [self locationLabelFont];
  }
}

#pragma mark - UIAccessibilityContainer

- (NSArray*)accessibilityElements {
  return self.accessibleElements;
}

- (NSInteger)accessibilityElementCount {
  return self.accessibleElements.count;
}

- (id)accessibilityElementAtIndex:(NSInteger)index {
  return self.accessibleElements[index];
}

- (NSInteger)indexOfAccessibilityElement:(id)element {
  return [self.accessibleElements indexOfObject:element];
}

#pragma mark - private

- (void)updateAccessibility {
  if (self.securityLevelAccessibilityString.length > 0) {
    self.locationButton.accessibilityValue =
        [NSString stringWithFormat:@"%@ %@", self.locationLabel.text,
                                   self.securityLevelAccessibilityString];
  } else {
    self.locationButton.accessibilityValue =
        [NSString stringWithFormat:@"%@", self.locationLabel.text];
  }

  if (self.trailingButton.enabled) {
    [self.accessibleElements addObject:self.trailingButton];
  } else {
    [self.accessibleElements removeObject:self.trailingButton];
  }
}

// Returns the font size for the location label.
- (UIFont*)locationLabelFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityExtraLarge);
}

@end
