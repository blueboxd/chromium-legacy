// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASKBAR_TASKBAR_DECORATOR_WIN_H_
#define CHROME_BROWSER_TASKBAR_TASKBAR_DECORATOR_WIN_H_

#include <string>

#include "ui/gfx/native_widget_types.h"

class Profile;

namespace gfx {
class Image;
}

namespace taskbar {

// Error codes to create custom hresult for recording UMA metrics.
// Based on the official documentation
// https://docs.microsoft.com/en-us/windows/win32/com/codes-in-facility-itf?redirectedfrom=MSDN
// A custom code can choose any values between  0x0200-0xFFFF to
// not overlap with COM-defined errors.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CustomHresultCodes : int {
  kIsBrowserAbort = 0x400,
  kInvalidIcon = 0x401,
  kWindowInvisible = 0x402,
  kImageLoading = 0x403,
};

// Add a badge with the text |content| to the taskbar.
// |alt_text| will be read by screen readers.
// |uma_metric_name| will be used to log the result of the operation.
void DrawTaskbarDecorationString(gfx::NativeWindow window,
                                 const std::string& content,
                                 const std::string& alt_text,
                                 const char* uma_metric_name);

// Draws a scaled version of the avatar in |image| on the taskbar button
// associated with top level, visible |window|. Currently only implemented
// for Windows 7 and above.
// |uma_metric_name| will be used to log the result of the operation.
void DrawTaskbarDecoration(gfx::NativeWindow window,
                           const gfx::Image* image,
                           const char* uma_metric_name);

// Draws a taskbar icon for non-guest sessions, erases it otherwise. Note: This
// will clear any badge that has been set on the window.
// |uma_metric_name| will be used to log the result of the operation.
void UpdateTaskbarDecoration(Profile* profile,
                             gfx::NativeWindow window,
                             const char* uma_metric_name);

}  // namespace taskbar

#endif  // CHROME_BROWSER_TASKBAR_TASKBAR_DECORATOR_WIN_H_
