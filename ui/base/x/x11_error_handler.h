// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_ERROR_HANDLER_H_
#define UI_BASE_X_X11_ERROR_HANDLER_H_

#include "base/callback.h"
#include "base/component_export.h"

namespace ui {

// Sets null error handlers that just catch error messages.
COMPONENT_EXPORT(UI_BASE_X) void SetNullErrorHandlers();

// Sets error handlers that catch the error messages on ui thread, waits until
// errors are received on io thread, and stops the browser.
COMPONENT_EXPORT(UI_BASE_X)
void SetErrorHandlers(base::OnceCallback<void()> shutdown_cb);

// Unsets the error handlers.
COMPONENT_EXPORT(UI_BASE_X) void SetEmptyErrorHandlers();

}  // namespace ui

#endif  // UI_BASE_X_X11_ERROR_HANDLER_H_
