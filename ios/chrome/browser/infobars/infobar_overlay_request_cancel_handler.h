// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#include "base/scoped_observer.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request_cancel_handler.h"

// OverlayRequestCancelHandler that cancels its OverlayRequest when its InfoBar
// is removed from its InfoBarManager.
class InfobarOverlayRequestCancelHandler : public OverlayRequestCancelHandler {
 public:
  InfobarOverlayRequestCancelHandler(OverlayRequest* request,
                                     OverlayRequestQueue* queue,
                                     infobars::InfoBar* infobar);
  ~InfobarOverlayRequestCancelHandler() override;

 private:
  // Cancels the request for InfoBar removal.
  void Cancel();

  // Helper object that triggers cancellation when its InfoBar is removed from
  // its InfoBarManager.
  class RemovalObserver : public infobars::InfoBarManager::Observer {
   public:
    RemovalObserver(infobars::InfoBar* infobar,
                    InfobarOverlayRequestCancelHandler* cancel_handler);
    ~RemovalObserver() override;

   private:
    // infobars::InfoBarManager::Observer:
    void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

   private:
    infobars::InfoBar* infobar_ = nullptr;
    InfobarOverlayRequestCancelHandler* cancel_handler_ = nullptr;
    ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
        scoped_observer_;
  };

  RemovalObserver removal_observer_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_
