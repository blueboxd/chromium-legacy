// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridBottomToolbar {
  UIToolbar* _toolbar;
  UIBarButtonItem* _newTabButtonItem;
  UIBarButtonItem* _spaceItem;
  NSArray<NSLayoutConstraint*>* _compactConstraints;
  NSArray<NSLayoutConstraint*>* _floatingConstraints;
  TabGridNewTabButton* _smallNewTabButton;
  TabGridNewTabButton* _largeNewTabButton;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // The first time this moves to a superview, perform the view setup.
  if (newSuperview && self.subviews.count == 0) {
    [self setupViews];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLayout];
}

// Controls hit testing of the bottom toolbar. When the toolbar is transparent,
// only respond to tapping on the new tab button.
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  if ([self shouldUseCompactLayout]) {
    return [super pointInside:point withEvent:event];
  }
  // Only floating new tab button is tappable.
  return [_largeNewTabButton pointInside:[self convertPoint:point
                                                     toView:_largeNewTabButton]
                               withEvent:event];
}

#pragma mark - Public

// TODO(crbug.com/929981): "traitCollectionDidChange:" method won't get called
// when the view is not displayed, and in that case the only chance
// TabGridBottomToolbar can update its layout is when the TabGrid sets its
// "page" property in the
// "viewWillTransitionToSize:withTransitionCoordinator:" method. An early
// return for "self.page == page" can be added here since iOS 13 where the bug
// is fixed in UIKit.
- (void)setPage:(TabGridPage)page {
  _page = page;
  _smallNewTabButton.page = page;
  _largeNewTabButton.page = page;
  [self updateLayout];
}

- (void)setNewTabButtonTarget:(id)target action:(SEL)action {
  [_smallNewTabButton addTarget:target
                         action:action
               forControlEvents:UIControlEventTouchUpInside];
  [_largeNewTabButton addTarget:target
                         action:action
               forControlEvents:UIControlEventTouchUpInside];
}

- (void)hide {
  _smallNewTabButton.alpha = 0.0;
  _largeNewTabButton.alpha = 0.0;
}

- (void)show {
  _smallNewTabButton.alpha = 1.0;
  _largeNewTabButton.alpha = 1.0;
}

#pragma mark - Private

- (void)setupViews {
  // For Regular(V) x Compact(H) layout, display UIToolbar.
  _toolbar = [[UIToolbar alloc] init];
  _toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  _toolbar.barStyle = UIBarStyleBlack;
  _toolbar.translucent = YES;
  // Remove the border of UIToolbar.
  [_toolbar setShadowImage:[[UIImage alloc] init]
        forToolbarPosition:UIBarPositionAny];

  _leadingButton = [[UIBarButtonItem alloc] init];
  _leadingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _trailingButton = [[UIBarButtonItem alloc] init];
  _trailingButton.style = UIBarButtonItemStyleDone;
  _trailingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _spaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  UIImage* regularImage = [UIImage imageNamed:@"new_tab_toolbar_button"];
  UIImage* incognitoImage =
      [UIImage imageNamed:@"new_tab_toolbar_button_incognito"];
  _smallNewTabButton =
      [[TabGridNewTabButton alloc] initWithRegularImage:regularImage
                                         incognitoImage:incognitoImage];
  _smallNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _smallNewTabButton.page = self.page;

  _newTabButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:_smallNewTabButton];
  // Set UIBarButtonItem.image to get a built-in accessibility modal panel.
  // The panel will be shown when user long press on the button, under
  // accessibility font size. The image will be normalized into a bi-color
  // image, so the incognito image is suitable because it has a transparent
  // "+". Use the larger image for higher resolution.
  _newTabButtonItem.image = incognitoImage;
  _newTabButtonItem.title = _smallNewTabButton.accessibilityLabel;

  _compactConstraints = @[
    [_toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_toolbar.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor],
    [_toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ];

  // For other layout, display a floating new tab button.
  _largeNewTabButton = [[TabGridNewTabButton alloc]
      initWithRegularImage:[UIImage imageNamed:@"new_tab_floating_button"]
            incognitoImage:
                [UIImage imageNamed:@"new_tab_floating_button_incognito"]];
  _largeNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _largeNewTabButton.page = self.page;

  _floatingConstraints = @[
    [_largeNewTabButton.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_largeNewTabButton.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor
                       constant:-kTabGridFloatingButtonVerticalInset],
    [_largeNewTabButton.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kTabGridFloatingButtonHorizontalInset],
  ];
}

- (void)updateLayout {
  if ([self shouldUseCompactLayout]) {
    // For incognito/regular pages, display all 3 buttons;
    // For remote tabs page, only display new tab button.
    if (self.page == TabGridPageRemoteTabs) {
      [_toolbar setItems:@[ _spaceItem, self.trailingButton ]];
    } else {
      [_toolbar setItems:@[
        self.leadingButton, _spaceItem, _newTabButtonItem, _spaceItem,
        self.trailingButton
      ]];
    }

    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_largeNewTabButton removeFromSuperview];
    [self addSubview:_toolbar];
    [NSLayoutConstraint activateConstraints:_compactConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_compactConstraints];
    [_toolbar removeFromSuperview];

    if (self.page == TabGridPageRemoteTabs) {
      [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
      [_largeNewTabButton removeFromSuperview];
    } else {
      [self addSubview:_largeNewTabButton];
      [NSLayoutConstraint activateConstraints:_floatingConstraints];
    }
  }
}

// Returns YES if should use compact bottom toolbar layout.
- (BOOL)shouldUseCompactLayout {
  // TODO(crbug.com/929981): UIView's |traitCollection| can be wrong and
  // contradict the keyWindow's |traitCollection| because UIView's
  // |-traitCollectionDidChange:| is not properly called when the view rotates
  // while it is in a ViewController deeper in the ViewController hierarchy. Use
  // self.traitCollection since iOS 13 where the bug is fixed in UIKit.
  return UIApplication.sharedApplication.keyWindow.traitCollection
                 .verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         UIApplication.sharedApplication.keyWindow.traitCollection
                 .horizontalSizeClass == UIUserInterfaceSizeClassCompact;
}

@end
