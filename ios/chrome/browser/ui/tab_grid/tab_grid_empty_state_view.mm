// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_empty_state_view.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kVerticalMargin = 16;
}  // namespace

@interface TabGridEmptyStateView ()
@property(nonatomic, strong) UIView* container;
@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) NSLayoutConstraint* scrollViewHeight;
@property(nonatomic, copy, readonly) NSString* title;
@property(nonatomic, copy, readonly) NSString* body;
@end

@implementation TabGridEmptyStateView
// GridEmptyView properties.
@synthesize scrollViewContentInsets = _scrollViewContentInsets;

- (instancetype)initWithPage:(TabGridPage)page {
  if (self = [super initWithFrame:CGRectZero]) {
    switch (page) {
      case TabGridPageIncognitoTabs:
        _title = l10n_util::GetNSString(
            IDS_IOS_TAB_GRID_INCOGNITO_TABS_EMPTY_STATE_TITLE);
        _body = l10n_util::GetNSString(
            IDS_IOS_TAB_GRID_INCOGNITO_TABS_EMPTY_STATE_BODY);
        break;
      case TabGridPageRegularTabs:
        _title = l10n_util::GetNSString(
            IDS_IOS_TAB_GRID_REGULAR_TABS_EMPTY_STATE_TITLE);
        _body = l10n_util::GetNSString(
            IDS_IOS_TAB_GRID_REGULAR_TABS_EMPTY_STATE_BODY);
        break;
      case TabGridPageRemoteTabs:
        // No-op. Empty page.
        break;
    }
  }
  return self;
}

#pragma mark - Accessor

- (void)setScrollViewContentInsets:(UIEdgeInsets)scrollViewContentInsets {
  _scrollViewContentInsets = scrollViewContentInsets;
  self.scrollView.contentInset = scrollViewContentInsets;
  self.scrollViewHeight.constant =
      scrollViewContentInsets.top + scrollViewContentInsets.bottom;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (newSuperview) {
    // The first time this moves to a superview, perform the view setup.
    if (self.subviews.count == 0)
      [self setupViews];
    [self.container.widthAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.widthAnchor]
        .active = YES;
  }
}

#pragma mark - Private

- (void)setupViews {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  self.container = container;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrollView = scrollView;

  UILabel* topLabel = [[UILabel alloc] init];
  topLabel.translatesAutoresizingMaskIntoConstraints = NO;
  topLabel.text = self.title;
  topLabel.textColor = UIColorFromRGB(kTabGridEmptyStateTitleTextColor);
  topLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  topLabel.adjustsFontForContentSizeCategory = YES;
  topLabel.numberOfLines = 0;
  topLabel.textAlignment = NSTextAlignmentCenter;

  UILabel* bottomLabel = [[UILabel alloc] init];
  bottomLabel.translatesAutoresizingMaskIntoConstraints = NO;
  bottomLabel.text = self.body;
  bottomLabel.textColor = UIColorFromRGB(kTabGridEmptyStateBodyTextColor);
  bottomLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  bottomLabel.adjustsFontForContentSizeCategory = YES;
  bottomLabel.numberOfLines = 0;
  bottomLabel.textAlignment = NSTextAlignmentCenter;

  [container addSubview:topLabel];
  [container addSubview:bottomLabel];
  [scrollView addSubview:container];
  [self addSubview:scrollView];

  NSLayoutConstraint* scrollViewHeightConstraint = [scrollView.heightAnchor
      constraintEqualToAnchor:container.heightAnchor
                     constant:(self.scrollViewContentInsets.top +
                               self.scrollViewContentInsets.bottom)];
  scrollViewHeightConstraint.priority = UILayoutPriorityDefaultLow;
  scrollViewHeightConstraint.active = YES;
  self.scrollViewHeight = scrollViewHeightConstraint;

  [NSLayoutConstraint activateConstraints:@[
    [topLabel.topAnchor constraintEqualToAnchor:container.topAnchor
                                       constant:kVerticalMargin],
    [topLabel.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [topLabel.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [topLabel.bottomAnchor
        constraintEqualToAnchor:bottomLabel.topAnchor
                       constant:-kTabGridEmptyStateVerticalMargin],
    [bottomLabel.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [bottomLabel.trailingAnchor
        constraintEqualToAnchor:container.trailingAnchor],
    [bottomLabel.bottomAnchor constraintEqualToAnchor:container.bottomAnchor
                                             constant:-kVerticalMargin],

    [container.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [container.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],
    [container.centerXAnchor constraintEqualToAnchor:scrollView.centerXAnchor],

    [scrollView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [scrollView.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor],
    [scrollView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ]];
}

@end
