// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_INFOBAR_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mock version of InfobarBannerInteractionHandler.
class MockInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  MockInfobarBannerInteractionHandler();
  ~MockInfobarBannerInteractionHandler() override;

  MOCK_METHOD2(BannerVisibilityChanged,
               void(InfoBarIOS* infobar, bool visible));
  MOCK_METHOD1(MainButtonTapped, void(InfoBarIOS* infobar));
  MOCK_METHOD2(ShowModalButtonTapped,
               void(InfoBarIOS* infobar, web::WebState* web_state));
  MOCK_METHOD1(BannerDismissedByUser, void(InfoBarIOS* infobar));
};

// Mock version of InfobarDetailSheetInteractionHandler.
class MockInfobarDetailSheetInteractionHandler
    : public InfobarDetailSheetInteractionHandler {
 public:
  MockInfobarDetailSheetInteractionHandler();
  ~MockInfobarDetailSheetInteractionHandler() override;

  // TODO(crbug.com/1030357): Add mock interaction handling for detail sheets.
};

// Mock version of MockInfobarModalInteractionHandler.
class MockInfobarModalInteractionHandler
    : public InfobarModalInteractionHandler {
 public:
  MockInfobarModalInteractionHandler();
  ~MockInfobarModalInteractionHandler() override;

  // TODO(crbug.com/1030357): Add mock interaction handling for modals.
};

// InfobarModalInteractionHandler subclass that returns mock versions of the
// banner, detail sheet, and modal interaction handlers.
class MockInfobarInteractionHandler : public InfobarInteractionHandler {
 public:
  MockInfobarInteractionHandler();
  ~MockInfobarInteractionHandler() override;

  MockInfobarBannerInteractionHandler* mock_banner_handler() {
    return mock_banner_handler_;
  }
  MockInfobarDetailSheetInteractionHandler* mock_sheet_handler() {
    return mock_sheet_handler_;
  }
  MockInfobarModalInteractionHandler* mock_modal_handler() {
    return mock_modal_handler_;
  }

 private:
  // Pointers to the mock handlers passed to the InfobarInteractionHandler
  // constructor.  Guaranteed to be non-null for the lifetime of the object.
  MockInfobarBannerInteractionHandler* mock_banner_handler_ = nullptr;
  MockInfobarDetailSheetInteractionHandler* mock_sheet_handler_ = nullptr;
  MockInfobarModalInteractionHandler* mock_modal_handler_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_INFOBAR_INTERACTION_HANDLER_H_
