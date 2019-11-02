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

namespace {

// The steps of the wizard.
enum class WizardPage {
  ACCESSIBILITY,
  SCREEN_RECORDING,
  ALL_SET,
};

}  // namespace

@interface PermissionWizardController : NSWindowController

- (instancetype)initWithWindow:(NSWindow*)window;
- (void)hide;
- (void)initializeWindow;

@end

@implementation PermissionWizardController {
  NSTextField* _instructionText;
  NSButton* _cancelButton;
  NSButton* _nextButton;

  // The page of the wizard being shown.
  WizardPage _page;

  // Whether the relevant permission has been granted for the current page. If
  // YES, the user will be able to advance to the next page of the wizard.
  BOOL _hasPermission;
}

- (instancetype)initWithWindow:(NSWindow*)window {
  self = [super initWithWindow:(NSWindow*)window];
  if (self) {
    _page = WizardPage::ACCESSIBILITY;
  }
  return self;
}

- (void)hide {
  [self close];
}

- (void)initializeWindow {
  self.window.title =
      l10n_util::GetNSStringF(IDS_MAC_PERMISSION_WIZARD_TITLE,
                              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  _instructionText = [[[NSTextField alloc] init] autorelease];
  _instructionText.translatesAutoresizingMaskIntoConstraints = NO;
  _instructionText.drawsBackground = NO;
  _instructionText.bezeled = NO;
  _instructionText.editable = NO;
  _instructionText.preferredMaxLayoutWidth = 400;

  NSImageView* icon = [[[NSImageView alloc] init] autorelease];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  icon.image = [[NSApplication sharedApplication] applicationIconImage];

  _cancelButton = [[[NSButton alloc] init] autorelease];
  _cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  _cancelButton.buttonType = NSButtonTypeMomentaryPushIn;
  _cancelButton.bezelStyle = NSBezelStyleRegularSquare;
  _cancelButton.title =
      l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_CANCEL_BUTTON);
  _cancelButton.action = @selector(onCancel:);
  _cancelButton.target = self;

  _nextButton = [[[NSButton alloc] init] autorelease];
  _nextButton.translatesAutoresizingMaskIntoConstraints = NO;
  _nextButton.buttonType = NSButtonTypeMomentaryPushIn;
  _nextButton.bezelStyle = NSBezelStyleRegularSquare;
  _nextButton.action = @selector(onNext:);
  _nextButton.target = self;

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
  [buttonsStack addView:_cancelButton inGravity:NSStackViewGravityTrailing];
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

  [self updateUI];

  [self.window makeKeyAndOrderFront:NSApp];
  [self.window center];
}

- (void)onCancel:(id)sender {
  [self hide];
}

- (void)onNext:(id)sender {
  NOTIMPLEMENTED();
}

// Updates the dialog controls according to the object's state.
- (void)updateUI {
  // TODO(lambroslambrou): Parameterize the name of the app needing permission.
  switch (_page) {
    case WizardPage::ACCESSIBILITY:
      _instructionText.stringValue = l10n_util::GetNSStringF(
          IDS_ACCESSIBILITY_PERMISSION_DIALOG_BODY_TEXT,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          l10n_util::GetStringUTF16(
              IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON),
          base::SysNSStringToUTF16(@"ChromeRemoteDesktopHost"));
      break;
    case WizardPage::SCREEN_RECORDING:
      _instructionText.stringValue = l10n_util::GetNSStringF(
          IDS_SCREEN_RECORDING_PERMISSION_DIALOG_BODY_TEXT,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          l10n_util::GetStringUTF16(
              IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON),
          base::SysNSStringToUTF16(@"ChromeRemoteDesktopHost"));
      break;
    case WizardPage::ALL_SET:
      _instructionText.stringValue =
          l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_FINAL_TEXT);
      break;
    default:
      NOTREACHED();
  }
  _cancelButton.hidden = (_page == WizardPage::ALL_SET);
  [self updateNextButton];
}

// Updates |_nextButton| according to the object's state.
- (void)updateNextButton {
  if (_page == WizardPage::ALL_SET) {
    _nextButton.title =
        l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_OK_BUTTON);
    return;
  }

  if (_hasPermission) {
    _nextButton.title =
        l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_NEXT_BUTTON);
    return;
  }

  // Permission is not granted, so show the appropriate launch text.
  switch (_page) {
    case WizardPage::ACCESSIBILITY:
      _nextButton.title = l10n_util::GetNSString(
          IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON);
      break;
    case WizardPage::SCREEN_RECORDING:
      _nextButton.title = l10n_util::GetNSString(
          IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON);
      break;
    default:
      NOTREACHED();
  }
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
