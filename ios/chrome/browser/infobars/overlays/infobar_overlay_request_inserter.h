// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_INSERTER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_INSERTER_H_

#include <map>
#include <memory>

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory_impl.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_type.h"
#include "ios/web/public/web_state_user_data.h"

namespace infobars {
class InfoBar;
}
class InfobarModalCompletionNotifier;
class InfobarOverlayRequestFactory;
class OverlayRequestQueue;
namespace web {
class WebState;
}

// Helper object that creates OverlayRequests for InfoBars and inserts them into
// a WebState's OverlayRequestQueues.
class InfobarOverlayRequestInserter
    : public web::WebStateUserData<InfobarOverlayRequestInserter> {
 public:
  // Creates an inserter for |web_state| that uses |request_factory| to create
  // inserted requests.
  static void CreateForWebState(
      web::WebState* web_state,
      std::unique_ptr<InfobarOverlayRequestFactory> request_factory =
          std::make_unique<InfobarOverlayRequestFactoryImpl>());

  ~InfobarOverlayRequestInserter() override;

  // Creates an OverlayRequest for |type| configured with |infobar| and adds it
  // to the back of the OverlayRequestQueue at |type|'s modality.
  void AddOverlayRequest(infobars::InfoBar* infobar,
                         InfobarOverlayType type) const;

  // Creates an OverlayRequest for |type| configured with |infobar| and adds it
  // at |index| to the OverlayRequestQueue at |type|'s modality.  |index| must
  // be less than or equal to the size of the queue.
  void InsertOverlayRequest(infobars::InfoBar* infobar,
                            InfobarOverlayType type,
                            size_t index) const;

 private:
  friend class web::WebStateUserData<InfobarOverlayRequestInserter>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Constructor for an inserter that uses |factory| to construct
  // OverlayRequests to insert into |web_state|'s OverlayRequestQueues.  Both
  // |web_state| and |factory| must be non-null.
  InfobarOverlayRequestInserter(
      web::WebState* web_state,
      std::unique_ptr<InfobarOverlayRequestFactory> factory);

  // The WebState whose queues are being inserted into.
  web::WebState* web_state_ = nullptr;
  // The infobar modal completion notifier.
  std::unique_ptr<InfobarModalCompletionNotifier> modal_completion_notifier_;
  // The factory used to create OverlayRequests.
  std::unique_ptr<InfobarOverlayRequestFactory> request_factory_;
  // Map of the OverlayRequestQueues to use for each InfobarOverlayType.
  std::map<InfobarOverlayType, OverlayRequestQueue*> queues_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_INSERTER_H_
