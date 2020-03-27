// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/security_token_pin/error_generator.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/base/ui_base_paths.h"

namespace chromeos {
namespace security_token_pin {

class SecurityTokenPinErrorGeneratorTest : public testing::Test {
 protected:
  SecurityTokenPinErrorGeneratorTest() { InitI18n(); }

  ~SecurityTokenPinErrorGeneratorTest() override {
    ui::ResourceBundle::CleanupSharedInstance();
  }

 private:
  // Initializes the i18n stack and loads the necessary strings. Uses a specific
  // locale, so that the tests can compare against golden strings without
  // depending on the environment.
  void InitI18n() {
    base::i18n::SetICUDefaultLocale("en_US");

    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    base::FilePath dir_module_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &dir_module_path));
    base::FilePath chromeos_test_strings_path =
        dir_module_path.Append(FILE_PATH_LITERAL("chromeos_test_strings.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        chromeos_test_strings_path, ui::SCALE_FACTOR_NONE);
  }
};

// Tests that an empty message is returned when there's neither an error nor the
// number of attempts left.
TEST_F(SecurityTokenPinErrorGeneratorTest, NoError) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kNone, /*attempts_left=*/-1,
                                 /*accept_input=*/true),
            base::string16());
}

// Tests the message for the kInvalidPin error.
TEST_F(SecurityTokenPinErrorGeneratorTest, InvalidPin) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPin, /*attempts_left=*/-1,
                                 /*accept_input=*/true),
            base::ASCIIToUTF16("Invalid PIN."));
}

// Tests the message for the kInvalidPuk error.
TEST_F(SecurityTokenPinErrorGeneratorTest, InvalidPuk) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPuk, /*attempts_left=*/-1,
                                 /*accept_input=*/true),
            base::ASCIIToUTF16("Invalid PUK."));
}

// Tests the message for the kMaxAttemptsExceeded error.
TEST_F(SecurityTokenPinErrorGeneratorTest, MaxAttemptsExceeded) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kMaxAttemptsExceeded,
                                 /*attempts_left=*/-1,
                                 /*accept_input=*/false),
            base::ASCIIToUTF16("Maximum allowed attempts exceeded."));
}

// Tests the message for the kMaxAttemptsExceeded error with the zero number of
// attempts left.
TEST_F(SecurityTokenPinErrorGeneratorTest, MaxAttemptsExceededZeroAttempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kMaxAttemptsExceeded,
                                 /*attempts_left=*/0,
                                 /*accept_input=*/false),
            base::ASCIIToUTF16("Maximum allowed attempts exceeded."));
}

// Tests the message for the kUnknown error.
TEST_F(SecurityTokenPinErrorGeneratorTest, UnknownError) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kUnknown, /*attempts_left=*/-1,
                                 /*accept_input=*/true),
            base::ASCIIToUTF16("Unknown error."));
}

// Tests the message when the number of attempts left is given.
TEST_F(SecurityTokenPinErrorGeneratorTest, Attempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kNone, /*attempts_left=*/3,
                                 /*accept_input=*/true),
            base::ASCIIToUTF16("3 attempts left"));
}

// Tests that an empty message is returned when the number of attempts is given
// such that, heuristically, it's too big to be displayed for the user.
TEST_F(SecurityTokenPinErrorGeneratorTest, HiddenAttempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kNone, /*attempts_left=*/4,
                                 /*accept_input=*/true),
            base::string16());
}

// Tests the message for the kInvalidPin error with the number of attempts left.
TEST_F(SecurityTokenPinErrorGeneratorTest, InvalidPinWithAttempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPin, /*attempts_left=*/3,
                                 /*accept_input=*/true),
            base::ASCIIToUTF16("Invalid PIN. 3 attempts left"));
}

// Tests the message for the kInvalidPin error with such a number of attempts
// left that, heuristically, shouldn't be displayed to the user.
TEST_F(SecurityTokenPinErrorGeneratorTest, InvalidPinWithHiddenAttempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPin, /*attempts_left=*/4,
                                 /*accept_input=*/true),
            base::ASCIIToUTF16("Invalid PIN."));
}

}  // namespace security_token_pin
}  // namespace chromeos
