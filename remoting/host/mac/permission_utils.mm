// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/mac/permission_utils.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr int kMinDialogWidthPx = 650;

// The name of the host service as it appears in the system's Accessibility
// permission dialog.
constexpr NSString* kHostServiceName = @"remoting_me2me_host";

void ShowPermissionDialog() {
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert setMessageText:l10n_util::GetNSString(
                            IDS_ACCESSIBILITY_PERMISSION_DIALOG_TITLE)];
  [alert setInformativeText:
             l10n_util::GetNSStringF(
                 IDS_ACCESSIBILITY_PERMISSION_DIALOG_BODY_TEXT,
                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                 l10n_util::GetStringUTF16(
                     IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON),
                 base::SysNSStringToUTF16(kHostServiceName))];
  [alert
      addButtonWithTitle:l10n_util::GetNSString(
                             IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON)];
  [alert addButtonWithTitle:
             l10n_util::GetNSString(
                 IDS_ACCESSIBILITY_PERMISSION_DIALOG_NOT_NOW_BUTTON)];

  // Increase the alert width so the title doesn't wrap and the body text is
  // less scrunched.  Note that we only want to set a min-width, we don't
  // want to shrink the dialog if it is already larger than our min value.
  NSWindow* alert_window = [alert window];
  NSRect frame = [alert_window frame];
  if (frame.size.width < kMinDialogWidthPx)
    frame.size.width = kMinDialogWidthPx;
  [alert_window setFrame:frame display:YES];

  [alert setAlertStyle:NSAlertStyleWarning];
  [alert_window makeKeyWindow];
  if ([alert runModal] == NSAlertFirstButtonReturn) {
    // Launch the Security and Preferences pane with Accessibility selected.
    [[NSWorkspace sharedWorkspace]
        openURL:
            [NSURL URLWithString:@"x-apple.systempreferences:com.apple."
                                 @"preference.security?Privacy_Accessibility"]];
  }
}
}  // namespace

namespace remoting {
namespace mac {

void PromptUserToChangeTrustStateIfNeeded(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (AXIsProcessTrusted())
    return;

  LOG(WARNING) << "AXIsProcessTrusted returned false, requesting "
               << "permission from user to allow input injection.";

  task_runner->PostTask(FROM_HERE, base::BindOnce(&ShowPermissionDialog));
}

}  // namespace mac
}  // namespace remoting
