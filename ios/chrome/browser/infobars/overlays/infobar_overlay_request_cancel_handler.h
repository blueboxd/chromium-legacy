// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#include "base/scoped_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/public/overlay_request_cancel_handler.h"

class InfobarOverlayRequestInserter;

// OverlayRequestCancelHandler that cancels its OverlayRequest when its InfoBar
// is removed from its InfoBarManager.
class InfobarOverlayRequestCancelHandler : public OverlayRequestCancelHandler {
 public:
  InfobarOverlayRequestCancelHandler(
      OverlayRequest* request,
      OverlayRequestQueue* queue,
      InfobarOverlayType type,
      const InfobarOverlayRequestInserter* inserter);
  ~InfobarOverlayRequestCancelHandler() override;

 private:
  // Returns the InfoBar that the corresponding request was configured with.
  infobars::InfoBar* infobar() const { return infobar_; }

  // Cancels the request for InfoBar removal.
  void Cancel();

  // Inserts an OverlayRequest for |replacement| behind the cancel handler's
  // request.
  void InsertReplacementRequest(infobars::InfoBar* replacement);

  // Helper object that triggers cancellation when its InfoBar is removed from
  // its InfoBarManager.
  class RemovalObserver : public infobars::InfoBarManager::Observer {
   public:
    RemovalObserver(InfobarOverlayRequestCancelHandler* cancel_handler);
    ~RemovalObserver() override;

   private:
    // infobars::InfoBarManager::Observer:
    void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
    void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                           infobars::InfoBar* new_infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

   private:
    InfobarOverlayRequestCancelHandler* cancel_handler_ = nullptr;
    ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
        scoped_observer_;
  };

  InfobarOverlayType type_;
  const InfobarOverlayRequestInserter* inserter_ = nullptr;
  infobars::InfoBar* infobar_ = nullptr;
  RemovalObserver removal_observer_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_CANCEL_HANDLER_H_
