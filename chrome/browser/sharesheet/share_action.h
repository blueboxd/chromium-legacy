// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARE_ACTION_H_
#define CHROME_BROWSER_SHARESHEET_SHARE_ACTION_H_

#include "base/strings/string16.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"

namespace sharesheet {

// An interface implemented by each ShareAction.
class ShareAction {
 public:
  virtual ~ShareAction() = default;

  virtual const base::string16 GetActionName() = 0;

  virtual const gfx::Image GetActionIcon() = 0;

  // LaunchAction should synchronously create all UI needed and fill
  // the |root_view|. Methods on |controller| can be used to inform
  // the sharesheet about the lifecycle of the ShareAction.
  //
  // |root_view| is a container within the larger share_sheet which should act
  // as the parent view for ShareAction views. It is guaranteed that
  // |root_view| and |controller| will stay alive and visible until either
  // ShareAction::OnClosing is called, or the ShareAction calls
  // |controller|->ShareActionCompleted().
  virtual void LaunchAction(SharesheetController* controller,
                            views::View* root_view) = 0;

  // OnClosing informs the ShareAction when the sharesheet with |controller| is
  // closed. This occurs when the user presses the back button out of the share
  // action view or closes the sharesheet. All processes in ShareAction should
  // shutdown when OnClosing is called, and not use |root_view| or |controller|
  // once the method completes as they will be destroyed.
  virtual void OnClosing(SharesheetController* controller) = 0;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARE_ACTION_H_
