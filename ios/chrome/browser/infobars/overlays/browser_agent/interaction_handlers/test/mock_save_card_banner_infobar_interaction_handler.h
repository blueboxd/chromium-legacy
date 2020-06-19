// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_CARD_BANNER_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_CARD_BANNER_INFOBAR_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_banner_interaction_handler.h"

#include <string.h>

#include "testing/gmock/include/gmock/gmock.h"

class InfoBarIOS;

// Mock version of SaveCardInfobarBannerInteractionHandler for use in tests.
class MockSaveCardInfobarBannerInteractionHandler
    : public SaveCardInfobarBannerInteractionHandler {
 public:
  MockSaveCardInfobarBannerInteractionHandler();
  ~MockSaveCardInfobarBannerInteractionHandler() override;

  MOCK_METHOD4(SaveCredentials,
               void(InfoBarIOS* infobar,
                    base::string16 cardholder_name,
                    base::string16 expiration_date_month,
                    base::string16 expiration_date_year));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_CARD_BANNER_INFOBAR_INTERACTION_HANDLER_H_
