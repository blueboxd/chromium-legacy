// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_REQUEST_CANCEL_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_cancel_handler.h"

#include "base/scoped_observer.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"

class InfobarOverlayRequestInserter;
class OverlayRequestQueue;

namespace translate_infobar_overlays {

// A cancel handler for Translate Infobar placeholder requests that observes the
// TranslateInfobarDelegate and cancels the request when the translation
// finishes.
class PlaceholderRequestCancelHandler
    : public InfobarOverlayRequestCancelHandler {
 public:
  // Constructor for a handler that cancels |request| of |translate_infobar|
  // from |queue| and insert a banner request into |inserter| when translation
  // finishes. |inserter| must be non-null.
  PlaceholderRequestCancelHandler(OverlayRequest* request,
                                  OverlayRequestQueue* queue,
                                  InfobarOverlayRequestInserter* inserter,
                                  InfoBarIOS* translate_infobar);
  ~PlaceholderRequestCancelHandler() override;

 private:
  // Observer of TranslateInfobarDelegate that informs the
  // PlaceholderRequestCancelHandler about changes to the TranslateStep.
  class TranslateDelegateObserver
      : public translate::TranslateInfoBarDelegate::Observer {
   public:
    TranslateDelegateObserver(translate::TranslateInfoBarDelegate* delegate,
                              PlaceholderRequestCancelHandler* cancel_handler);
    ~TranslateDelegateObserver() override;

    // translate::TranslateInfoBarDelegate::Observer.
    void OnTranslateStepChanged(
        translate::TranslateStep step,
        translate::TranslateErrors::Type error_type) override;
    bool IsDeclinedByUser() override;
    void OnTranslateInfoBarDelegateDestroyed(
        translate::TranslateInfoBarDelegate* delegate) override;

    // The PlaceholderRequestCancelHandler.
    PlaceholderRequestCancelHandler* cancel_handler_ = nil;
    // Scoped observer that facilitates observing a TranslateInfoBarDelegate.
    ScopedObserver<translate::TranslateInfoBarDelegate,
                   translate::TranslateInfoBarDelegate::Observer>
        scoped_observer_;
  };

  // Notify the cancel handler of a change to the current TranslateStep.
  void TranslateStepChanged(translate::TranslateStep step);

  // Inserter to add requests to.
  InfobarOverlayRequestInserter* inserter_ = nullptr;
  TranslateDelegateObserver translate_delegate_observer_;
};

}  // namespace translate_infobar_overlays

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_REQUEST_CANCEL_HANDLER_H_
