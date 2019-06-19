// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_controller_impl.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/mock_address_accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/password_manager/password_accessory_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using autofill::AccessoryAction;
using autofill::mojom::FillingStatus;
using autofill::mojom::FocusedFieldType;
using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::StrictMock;
using FillingSource = ManualFillingController::FillingSource;

constexpr char kExampleSite[] = "https://example.com";

class MockPasswordAccessoryController : public PasswordAccessoryController {
 public:
  MOCK_METHOD2(
      SavePasswordsForOrigin,
      void(const std::map<base::string16, const autofill::PasswordForm*>&,
           const url::Origin&));
  MOCK_METHOD1(OnFilledIntoFocusedField, void(FillingStatus));
  MOCK_METHOD2(RefreshSuggestionsForField, void(FocusedFieldType, bool));
  MOCK_METHOD1(OnGenerationRequested, void(bool));
  MOCK_METHOD0(DidNavigateMainFrame, void());
  MOCK_METHOD2(GetFavicon,
               void(int, base::OnceCallback<void(const gfx::Image&)>));
  MOCK_METHOD1(OnFillingTriggered, void(const autofill::UserInfo::Field&));
  MOCK_METHOD1(OnOptionSelected, void(AccessoryAction selected_action));
};

autofill::AccessorySheetData dummy_accessory_sheet_data() {
  constexpr char kExampleAccessorySheetDataTitle[] = "Example title";
  return autofill::AccessorySheetData(
      autofill::AccessoryTabType::PASSWORDS,
      base::ASCIIToUTF16(kExampleAccessorySheetDataTitle));
}

}  // namespace

class ManualFillingControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ManualFillingControllerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kExampleSite));
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(),
        std::make_unique<StrictMock<MockManualFillingView>>());
    NavigateAndCommit(GURL(kExampleSite));
  }

  ManualFillingControllerImpl* controller() {
    return ManualFillingControllerImpl::FromWebContents(web_contents());
  }

  MockManualFillingView* view() {
    return static_cast<MockManualFillingView*>(controller()->view());
  }

 protected:
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
};

TEST_F(ManualFillingControllerTest, IsNotRecreatedForSameWebContents) {
  ManualFillingController* initial_controller =
      ManualFillingControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  ManualFillingControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(ManualFillingControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

// TODO(fhorschig): Check for recorded metrics here or similar to this.
TEST_F(ManualFillingControllerTest, ClosesViewWhenRefreshingSuggestions) {
  // Ignore Items - only the closing calls are interesting here.
  EXPECT_CALL(*view(), OnItemsAvailable(_)).Times(AnyNumber());

  EXPECT_CALL(*view(), CloseAccessorySheet());
  EXPECT_CALL(*view(), SwapSheetWithKeyboard())
      .Times(0);  // Don't touch the keyboard!
  controller()->RefreshSuggestionsForField(FocusedFieldType::kUnfillableElement,
                                           dummy_accessory_sheet_data());
}

// TODO(fhorschig): Check for recorded metrics here or similar to this.
TEST_F(ManualFillingControllerTest,
       SwapSheetWithKeyboardWhenRefreshingSuggestions) {
  // Ignore Items - only the closing calls are interesting here.
  EXPECT_CALL(*view(), OnItemsAvailable(_)).Times(AnyNumber());

  EXPECT_CALL(*view(), CloseAccessorySheet()).Times(0);
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->RefreshSuggestionsForField(FocusedFieldType::kFillableTextField,
                                           dummy_accessory_sheet_data());
}

// TODO(fhorschig): Check for recorded metrics here or similar to this.
TEST_F(ManualFillingControllerTest, ClosesViewOnSuccessfullFillingOnly) {
  // If the filling wasn't successful, no call is expected.
  EXPECT_CALL(*view(), CloseAccessorySheet()).Times(0);
  EXPECT_CALL(*view(), SwapSheetWithKeyboard()).Times(0);
  controller()->OnFilledIntoFocusedField(FillingStatus::ERROR_NOT_ALLOWED);
  controller()->OnFilledIntoFocusedField(FillingStatus::ERROR_NO_VALID_FIELD);

  // If the filling completed successfully, let the view know.
  EXPECT_CALL(*view(), SwapSheetWithKeyboard());
  controller()->OnFilledIntoFocusedField(FillingStatus::SUCCESS);
}

TEST_F(ManualFillingControllerTest, RelaysShowAndHideKeyboardAccessory) {
  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible());
  controller()->ShowWhenKeyboardIsVisible(FillingSource::PASSWORD_FALLBACKS);
  EXPECT_CALL(*view(), Hide());
  controller()->DeactivateFillingSource(FillingSource::PASSWORD_FALLBACKS);
}

TEST_F(ManualFillingControllerTest, RelaysShowTouchToFillSheet) {
  EXPECT_CALL(*view(), OnItemsAvailable(dummy_accessory_sheet_data()));
  EXPECT_CALL(*view(), ShowTouchToFillSheet);
  controller()->ShowTouchToFillSheet(dummy_accessory_sheet_data());
}

TEST_F(ManualFillingControllerTest, HidesAccessoryWhenAllSourcesRequestedIt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillKeyboardAccessory);
  EXPECT_CALL(*view(), ShowWhenKeyboardIsVisible()).Times(3);
  controller()->ShowWhenKeyboardIsVisible(FillingSource::PASSWORD_FALLBACKS);
  controller()->ShowWhenKeyboardIsVisible(FillingSource::AUTOFILL);
  // This duplicate call accounts for a single, visible source.
  controller()->ShowWhenKeyboardIsVisible(FillingSource::PASSWORD_FALLBACKS);

  // Hiding just one of two active filling sources won't have any effect.
  EXPECT_CALL(*view(), Hide()).Times(0);
  controller()->DeactivateFillingSource(FillingSource::PASSWORD_FALLBACKS);
  testing::Mock::VerifyAndClearExpectations(view());

  // Hiding the remaining second source will result in the view being hidden.
  EXPECT_CALL(*view(), Hide()).Times(1);
  controller()->DeactivateFillingSource(FillingSource::AUTOFILL);
}

TEST_F(ManualFillingControllerTest, OnAutomaticGenerationStatusChanged) {
  EXPECT_CALL(*view(), OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationStatusChanged(true);

  EXPECT_CALL(*view(), OnAutomaticGenerationStatusChanged(false));
  controller()->OnAutomaticGenerationStatusChanged(false);
}

TEST_F(ManualFillingControllerTest, OnFillingTriggered) {
  const char kTextToFill[] = "TextToFill";
  const base::string16 text_to_fill(base::ASCIIToUTF16(kTextToFill));
  const autofill::UserInfo::Field field(text_to_fill, text_to_fill, false,
                                        true);

  EXPECT_CALL(mock_pwd_controller_, OnFillingTriggered(field));
  controller()->OnFillingTriggered(autofill::AccessoryTabType::PASSWORDS,
                                   field);
}

TEST_F(ManualFillingControllerTest, ForwardsPasswordManagingToController) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_PASSWORDS));
  controller()->OnOptionSelected(AccessoryAction::MANAGE_PASSWORDS);
}

TEST_F(ManualFillingControllerTest, ForwardsPasswordGenerationToController) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}

TEST_F(ManualFillingControllerTest, ForwardsAddressManagingToController) {
  EXPECT_CALL(mock_address_controller_,
              OnOptionSelected(AccessoryAction::MANAGE_ADDRESSES));
  controller()->OnOptionSelected(AccessoryAction::MANAGE_ADDRESSES);
}

TEST_F(ManualFillingControllerTest, OnAutomaticGenerationRequested) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_AUTOMATIC));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_AUTOMATIC);
}

TEST_F(ManualFillingControllerTest, OnManualGenerationRequested) {
  EXPECT_CALL(mock_pwd_controller_,
              OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}

TEST_F(ManualFillingControllerTest, GetFavicon) {
  constexpr int kIconSize = 75;
  auto icon_callback = base::BindOnce([](const gfx::Image&) {});

  EXPECT_CALL(mock_pwd_controller_, GetFavicon(kIconSize, _));
  controller()->GetFavicon(kIconSize, std::move(icon_callback));
}
