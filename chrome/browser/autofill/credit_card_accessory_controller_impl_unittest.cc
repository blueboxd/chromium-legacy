// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::_;
using testing::SaveArg;

namespace autofill {
namespace {

AccessorySheetData::Builder CreditCardAccessorySheetDataBuilder() {
  return AccessorySheetData::Builder(
             AccessoryTabType::CREDIT_CARDS,
             l10n_util::GetStringUTF16(
                 IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE))
      .AppendFooterCommand(
          l10n_util::GetStringUTF16(
              IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_ALL_ADDRESSES_LINK),
          AccessoryAction::MANAGE_CREDIT_CARDS);
}

}  // namespace

class CreditCardAccessoryControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CreditCardAccessoryControllerTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    CreditCardAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_mf_controller_.AsWeakPtr(), &data_manager_);

    data_manager_.SetPrefService(profile_.GetPrefs());
  }

  void TearDown() override {
    data_manager_.SetPrefService(nullptr);
    data_manager_.ClearCreditCards();
  }

  CreditCardAccessoryController* controller() {
    return CreditCardAccessoryControllerImpl::FromWebContents(web_contents());
  }

 protected:
  testing::NiceMock<MockManualFillingController> mock_mf_controller_;
  autofill::TestPersonalDataManager data_manager_;
  TestingProfile profile_;
};

TEST_F(CreditCardAccessoryControllerTest, RefreshSuggestionsForField) {
  autofill::CreditCard card = test::GetCreditCard();
  data_manager_.AddCreditCard(card);

  autofill::AccessorySheetData result(autofill::AccessoryTabType::CREDIT_CARDS,
                                      base::string16());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestions(_))
      .WillOnce(SaveArg<0>(&result));

  auto* cc_controller = controller();
  ASSERT_TRUE(cc_controller);
  cc_controller->RefreshSuggestions();

  ASSERT_EQ(
      result,
      CreditCardAccessorySheetDataBuilder()
          .AddUserInfo()
          .AppendField(card.ObfuscatedLastFourDigits(),
                       card.ObfuscatedLastFourDigits(), card.guid(),
                       /*is_obfuscated=*/false,
                       /*selectable=*/true)
          .AppendSimpleField(card.ExpirationMonthAsString())
          .AppendSimpleField(card.Expiration4DigitYearAsString())
          .AppendSimpleField(card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL))
          .Build());
}

}  // namespace autofill
