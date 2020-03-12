// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_site_security_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kHorizontalSpacing = 16.0f;

const CGFloat kVerticalSpacing = 20.0f;

const CGFloat kIconSize = 20.0f;

}  // namespace

@interface PageInfoSiteSecurityViewController ()

@property(nonatomic, strong)
    PageInfoSiteSecurityDescription* pageInfoSecurityDescription;

@end

@implementation PageInfoSiteSecurityViewController

- (instancetype)initWitDescription:
    (PageInfoSiteSecurityDescription*)description {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _pageInfoSecurityDescription = description;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SITE_SECURITY);

  // ScrollView that contains site security information.
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];
  AddSameConstraints(self.view, scrollView);

  // Site security information.
  UILabel* securitySummary = [[UILabel alloc] init];
  securitySummary.textColor = UIColor.cr_labelColor;
  securitySummary.numberOfLines = 0;
  securitySummary.adjustsFontForContentSizeCategory = YES;
  securitySummary.text = self.pageInfoSecurityDescription.message;
  securitySummary.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  securitySummary.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:securitySummary];

  // Security icon.
  UIImage* securityIcon = [self.pageInfoSecurityDescription.image
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIImageView* securityIconView =
      [[UIImageView alloc] initWithImage:securityIcon];
  securityIconView.tintColor = UIColor.cr_labelColor;
  securityIconView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:securityIconView];

  // Website URL.
  UILabel* securityURL = [[UILabel alloc] init];
  securityURL.textColor = UIColor.cr_labelColor;
  securityURL.adjustsFontForContentSizeCategory = YES;
  securityURL.text = self.pageInfoSecurityDescription.title;
  securityURL.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  securityURL.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:securityURL];

  // In some cases the icon can be nil.
  // e.g, if the URL scheme corresponds to chrome webUI, the icon is not set.
  CGFloat iconSize = self.pageInfoSecurityDescription.image ? kIconSize : 0;
  CGFloat iconToURLMargin =
      self.pageInfoSecurityDescription.image ? kHorizontalSpacing : 0;

  NSArray* constraints = @[
    // SecuritySummary constraints.
    [securitySummary.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalSpacing],
    [securitySummary.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kHorizontalSpacing],
    [securitySummary.topAnchor constraintEqualToAnchor:scrollView.topAnchor
                                              constant:kVerticalSpacing],

    // SecurityIcon constraints.
    [securityIconView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalSpacing],
    [securityIconView.topAnchor
        constraintGreaterThanOrEqualToAnchor:securitySummary.bottomAnchor
                                    constant:kVerticalSpacing],
    [securityIconView.heightAnchor constraintEqualToConstant:iconSize],
    [securityIconView.widthAnchor
        constraintEqualToAnchor:securityIconView.heightAnchor],

    [securityIconView.bottomAnchor
        constraintLessThanOrEqualToAnchor:scrollView.bottomAnchor
                                 constant:-kVerticalSpacing],

    // securityURL constraints.
    [securityURL.leadingAnchor
        constraintEqualToAnchor:securityIconView.trailingAnchor
                       constant:iconToURLMargin],
    [securityURL.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kHorizontalSpacing],
    [securityURL.topAnchor
        constraintGreaterThanOrEqualToAnchor:securitySummary.bottomAnchor
                                    constant:kVerticalSpacing],
    [securityURL.centerYAnchor
        constraintEqualToAnchor:securityIconView.centerYAnchor],
    [securityURL.bottomAnchor
        constraintLessThanOrEqualToAnchor:scrollView.bottomAnchor
                                 constant:-kVerticalSpacing],
  ];
  [NSLayoutConstraint activateConstraints:constraints];

  [self addButtonActionToScrollView:scrollView
                   securityIconView:securityIconView
                        securityURL:securityURL];
}

#pragma mark - Private

// Adds a button and his constrainsts based on the |securityIconView| and the
// |securityURL| to the |scrollView|.
- (void)addButtonActionToScrollView:(UIScrollView*)scrollView
                   securityIconView:(UIImageView*)securityIconView
                        securityURL:(UILabel*)securityURL {
  UIButton* buttonAction =
      [self buttonForAction:self.pageInfoSecurityDescription.buttonAction];
  // In some cases there is no actions.
  // e.g, if the URL scheme corresponds to chrome webUI, the button is not set.
  if (!buttonAction)
    return;

  buttonAction.titleLabel.adjustsFontForContentSizeCategory = YES;
  buttonAction.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];

  [buttonAction setTitleColor:[UIColor colorNamed:kBlueColor]
                     forState:UIControlStateNormal];
  buttonAction.translatesAutoresizingMaskIntoConstraints = NO;

  [scrollView addSubview:buttonAction];

  NSArray* buttonConstraints = @[
    [buttonAction.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalSpacing],
    [buttonAction.leadingAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .trailingAnchor
                                 constant:kHorizontalSpacing],
    [buttonAction.topAnchor
        constraintGreaterThanOrEqualToAnchor:securityIconView.bottomAnchor
                                    constant:kVerticalSpacing],
    [buttonAction.topAnchor
        constraintGreaterThanOrEqualToAnchor:securityURL.bottomAnchor
                                    constant:kVerticalSpacing],
    [buttonAction.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor
                                              constant:-kVerticalSpacing],
  ];
  [NSLayoutConstraint activateConstraints:buttonConstraints];
}

// Returns a button with title and action configured for |buttonAction|.
- (UIButton*)buttonForAction:(PageInfoSiteSecurityButtonAction)buttonAction {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  int messageId;
  switch (buttonAction) {
    case PageInfoSiteSecurityButtonActionNone:
      return nil;
    case PageInfoSiteSecurityButtonActionShowHelp:
      messageId = IDS_LEARN_MORE;
      [button addTarget:self.handler
                    action:@selector(showSecurityHelpPage)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    case PageInfoSiteSecurityButtonActionReload:
      messageId = IDS_IOS_PAGE_INFO_RELOAD;
      [button addTarget:self.handler
                    action:@selector(hidePageInfo)
          forControlEvents:UIControlEventTouchUpInside];
      [button addTarget:self.handler
                    action:@selector(reload)
          forControlEvents:UIControlEventTouchUpInside];
      break;
  };

  NSString* title = l10n_util::GetNSStringWithFixup(messageId);
  [button setTitle:title forState:UIControlStateNormal];
  return button;
}

@end
