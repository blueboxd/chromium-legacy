// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class OmniboxPopupView.  Each toolkit
// will implement the popup view differently, so that code is inherently
// platform specific.  However, the OmniboxPopupModel needs to do some
// communication with the view.  Since the model is shared between platforms,
// we need to define an interface that all view implementations will share.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_VIEW_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_VIEW_H_

#include <stddef.h>

#include "build/build_config.h"

class OmniboxPopupView {
 public:
  virtual ~OmniboxPopupView() = default;

  // Returns true if the popup is currently open.
  virtual bool IsOpen() const = 0;

  // Invalidates one line of the autocomplete popup.
  virtual void InvalidateLine(size_t line) = 0;

  // Invoked when the selected line changes. Either |old_selected_line| or
  // |new_selected_line| can be OmniboxPopupModel::kNoMatch. This method is
  // invoked by the model, and when it is, the view should consider the
  // LineState to have been reset to NORMAL.
  virtual void OnSelectedLineChanged(size_t old_selected_line,
                                     size_t new_selected_line) {}

  // Redraws the popup window to match any changes in the result set; this may
  // mean opening or closing the window.
  virtual void UpdatePopupAppearance() = 0;

  // Called to inform result view of button focus.
  virtual void ProvideButtonFocusHint(size_t line) = 0;

  // Notification that the icon used for the given match has been updated.
  virtual void OnMatchIconUpdated(size_t match_index) = 0;

  // This method is called when the view should cancel any active drag (e.g.
  // because the user pressed ESC). The view may or may not need to take any
  // action (e.g. releasing mouse capture).  Note that this can be called when
  // no drag is in progress.
  virtual void OnDragCanceled() = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_VIEW_H_
