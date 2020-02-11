// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"

#include "base/scoped_observer.h"
#import "ios/chrome/browser/infobars/overlays/infobar_modal_completion_notifier.h"

class InfobarOverlayRequestInserter;

// A cancel handler for Infobar banner UI OverlayRequests.
class InfobarBannerOverlayRequestCancelHandler
    : public InfobarOverlayRequestCancelHandler {
 public:
  // Constructor for a handler that cancels |request| from |queue|.  |inserter|
  // is used to insert replacement requests when an infobar is replaced.
  // |modal_completion_notifier| is used to detect the completion of any modal
  // UI that was presented from the banner.
  InfobarBannerOverlayRequestCancelHandler(
      OverlayRequest* request,
      OverlayRequestQueue* queue,
      const InfobarOverlayRequestInserter* inserter,
      InfobarModalCompletionNotifier* modal_completion_notifier);
  ~InfobarBannerOverlayRequestCancelHandler() override;

 private:
  // Helper object that triggers request cancellation for the completion of
  // modal requests created from the banner.
  class ModalCompletionObserver
      : public InfobarModalCompletionNotifier::Observer {
   public:
    ModalCompletionObserver(
        InfobarBannerOverlayRequestCancelHandler* cancel_handler,
        InfobarModalCompletionNotifier* completion_notifier,
        InfoBarIOS* infobar);
    ~ModalCompletionObserver() override;

   private:
    // InfobarModalCompletionNotifier::Observer:
    void InfobarModalsCompleted(InfobarModalCompletionNotifier* notifier,
                                InfoBarIOS* infobar) override;
    void InfobarModalCompletionNotifierDestroyed(
        InfobarModalCompletionNotifier* notifier) override;

    // The owning cancel handler.
    InfobarBannerOverlayRequestCancelHandler* cancel_handler_ = nullptr;
    // The infobar whose modal dismissals should trigger cancellation.
    InfoBarIOS* infobar_ = nullptr;
    ScopedObserver<InfobarModalCompletionNotifier,
                   InfobarModalCompletionNotifier::Observer>
        scoped_observer_;
  };

  // Cancels the request for modal completion.
  void CancelForModalCompletion();

  // InfobarOverlayRequestCancelHandler:
  void HandleReplacement(InfoBarIOS* replacement) override;

  // The inserter used to add replacement banner requests.
  const InfobarOverlayRequestInserter* inserter_ = nullptr;
  // The modal completion completion observer.
  ModalCompletionObserver modal_completion_observer_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_BANNER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
