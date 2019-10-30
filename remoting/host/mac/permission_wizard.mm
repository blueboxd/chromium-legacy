// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/permission_wizard.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface PermissionWizardController : NSWindowController

- (void)hide;
- (void)initializeWindow;
- (void)onNextButton;

@end

@implementation PermissionWizardController {
  NSTextField* _instructionText;
  NSButton* _nextButton;
}

- (void)hide {
  [self close];
}

- (void)initializeWindow {
  self.window.title =
      l10n_util::GetNSStringF(IDS_MAC_PERMISSION_WIZARD_TITLE,
                              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  // TODO(lambroslambrou): Parameterize the name of the app needing permission.
  _instructionText = [[[NSTextField alloc] init] autorelease];
  _instructionText.translatesAutoresizingMaskIntoConstraints = NO;
  _instructionText.stringValue = l10n_util::GetNSStringF(
      IDS_ACCESSIBILITY_PERMISSION_DIALOG_BODY_TEXT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(
          IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON),
      base::SysNSStringToUTF16(@"ChromeRemoteDesktopHost"));
  _instructionText.drawsBackground = NO;
  _instructionText.bezeled = NO;
  _instructionText.editable = NO;
  _instructionText.preferredMaxLayoutWidth = 400;

  NSImageView* icon = [[[NSImageView alloc] init] autorelease];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  icon.image = [[NSApplication sharedApplication] applicationIconImage];

  NSButton* cancelButton = [[[NSButton alloc] init] autorelease];
  cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  cancelButton.buttonType = NSButtonTypeMomentaryPushIn;
  cancelButton.bezelStyle = NSBezelStyleRegularSquare;
  cancelButton.title =
      l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_CANCEL_BUTTON);
  cancelButton.action = @selector(hide:);

  _nextButton = [[[NSButton alloc] init] autorelease];
  _nextButton.translatesAutoresizingMaskIntoConstraints = NO;
  _nextButton.buttonType = NSButtonTypeMomentaryPushIn;
  _nextButton.bezelStyle = NSBezelStyleRegularSquare;
  _nextButton.title =
      l10n_util::GetNSString(IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON);
  _nextButton.action = @selector(onNextButton:);

  NSStackView* iconAndTextStack = [[[NSStackView alloc] init] autorelease];
  iconAndTextStack.translatesAutoresizingMaskIntoConstraints = NO;
  iconAndTextStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  iconAndTextStack.alignment = NSLayoutAttributeTop;
  [iconAndTextStack addView:icon inGravity:NSStackViewGravityLeading];
  [iconAndTextStack addView:_instructionText
                  inGravity:NSStackViewGravityLeading];

  NSStackView* buttonsStack = [[[NSStackView alloc] init] autorelease];
  buttonsStack.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  [buttonsStack addView:cancelButton inGravity:NSStackViewGravityTrailing];
  [buttonsStack addView:_nextButton inGravity:NSStackViewGravityTrailing];

  NSStackView* mainStack = [[[NSStackView alloc] init] autorelease];
  mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  mainStack.orientation = NSUserInterfaceLayoutOrientationVertical;
  mainStack.spacing = 20;
  [mainStack addView:iconAndTextStack inGravity:NSStackViewGravityTop];
  [mainStack addView:buttonsStack inGravity:NSStackViewGravityBottom];

  [self.window.contentView addSubview:mainStack];

  NSDictionary* views = @{
    @"iconAndText" : iconAndTextStack,
    @"buttons" : buttonsStack,
    @"mainStack" : mainStack,
  };

  // Expand |iconAndTextStack| to match parent's width.
  [mainStack addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:@"H:|[iconAndText]|"
                                                    options:0
                                                    metrics:nil
                                                      views:views]];

  // Expand |buttonsStack| to match parent's width.
  [mainStack addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:@"H:|[buttons]|"
                                                    options:0
                                                    metrics:nil
                                                      views:views]];

  // Expand |mainStack| to fill the window's contentView (with standard margin).
  [self.window.contentView
      addConstraints:[NSLayoutConstraint
                         constraintsWithVisualFormat:@"H:|-[mainStack]-|"
                                             options:0
                                             metrics:nil
                                               views:views]];
  [self.window.contentView
      addConstraints:[NSLayoutConstraint
                         constraintsWithVisualFormat:@"V:|-[mainStack]-|"
                                             options:0
                                             metrics:nil
                                               views:views]];

  [self.window makeKeyAndOrderFront:NSApp];
  [self.window center];
}

- (void)onNextButton {
  NOTIMPLEMENTED();
}

@end

namespace remoting {
namespace mac {

class PermissionWizard::Impl {
 public:
  Impl() = default;
  ~Impl();

  void Start();

 private:
  PermissionWizardController* window_controller_ = nil;
};

PermissionWizard::Impl::~Impl() {
  // PermissionWizardController is responsible for releasing itself in its
  // windowWillClose: method.
  [window_controller_ hide];
  window_controller_ = nil;
}

void PermissionWizard::Impl::Start() {
  NSWindow* window =
      [[[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                   styleMask:NSWindowStyleMaskTitled
                                     backing:NSBackingStoreBuffered
                                       defer:NO] autorelease];
  window_controller_ =
      [[PermissionWizardController alloc] initWithWindow:window];
  [window_controller_ initializeWindow];
  [window_controller_ showWindow:nil];
}

PermissionWizard::PermissionWizard() = default;

PermissionWizard::~PermissionWizard() {
  ui_task_runner_->DeleteSoon(FROM_HERE, impl_.release());
}

void PermissionWizard::Start(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
  impl_ = std::make_unique<PermissionWizard::Impl>();
  ui_task_runner->PostTask(FROM_HERE,
                           base::BindOnce(&PermissionWizard::Impl::Start,
                                          base::Unretained(impl_.get())));
}

}  // namespace mac
}  // namespace remoting
