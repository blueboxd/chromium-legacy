// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_learn_more_view_controller.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/passwords/password_breach_presenter.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordBreachLearnMoreViewController ()

// Presenter used to dismiss this when finished.
@property(weak, readonly) id<PasswordBreachPresenter> presenter;

@end

@implementation PasswordBreachLearnMoreViewController

- (instancetype)initWithPresenter:(id<PasswordBreachPresenter>)presenter {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _presenter = presenter;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  UITextView* textView = [[UITextView alloc] init];
  textView.editable = NO;
  textView.textAlignment = NSTextAlignmentNatural;
  textView.adjustsFontForContentSizeCategory = YES;
  textView.text =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE);
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [self.view addSubview:textView];
  AddSameConstraints(textView, self.view.layoutMarginsGuide);

  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(didTapDoneButton)];
}

#pragma mark - Helpers

// Done button was tapped, inform the presenter.
- (void)didTapDoneButton {
  [self.presenter dismissLearnMore];
}

@end
