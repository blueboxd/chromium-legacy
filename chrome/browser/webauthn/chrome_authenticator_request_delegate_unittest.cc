// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {};

static constexpr char kRelyingPartyID[] = "example.com";

TEST_F(ChromeAuthenticatorRequestDelegateTest, TestTransportPrefType) {
  ChromeAuthenticatorRequestDelegate delegate(main_rfh(), kRelyingPartyID);
  EXPECT_FALSE(delegate.GetLastTransportUsed());
  delegate.UpdateLastTransportUsed(device::FidoTransportProtocol::kInternal);
  const auto transport = delegate.GetLastTransportUsed();
  ASSERT_TRUE(transport);
  EXPECT_EQ(device::FidoTransportProtocol::kInternal, transport);
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TestPairedDeviceAddressPreference) {
  static constexpr char kTestPairedDeviceAddress[] = "paired_device_address";
  static constexpr char kTestPairedDeviceAddress2[] = "paired_device_address2";

  ChromeAuthenticatorRequestDelegate delegate(main_rfh(), kRelyingPartyID);

  auto* const address_list = delegate.GetPreviouslyPairedFidoBleDeviceIds();
  ASSERT_TRUE(address_list);
  EXPECT_TRUE(address_list->empty());

  delegate.AddFidoBleDeviceToPairedList(kTestPairedDeviceAddress);
  const auto* updated_address_list =
      delegate.GetPreviouslyPairedFidoBleDeviceIds();
  ASSERT_TRUE(updated_address_list);
  ASSERT_EQ(1u, updated_address_list->GetSize());

  const auto& address_value = updated_address_list->GetList().at(0);
  ASSERT_TRUE(address_value.is_string());
  EXPECT_EQ(kTestPairedDeviceAddress, address_value.GetString());

  delegate.AddFidoBleDeviceToPairedList(kTestPairedDeviceAddress);
  const auto* address_list_with_duplicate_address_added =
      delegate.GetPreviouslyPairedFidoBleDeviceIds();
  ASSERT_TRUE(address_list_with_duplicate_address_added);
  EXPECT_EQ(1u, address_list_with_duplicate_address_added->GetSize());

  delegate.AddFidoBleDeviceToPairedList(kTestPairedDeviceAddress2);
  const auto* address_list_with_two_addresses =
      delegate.GetPreviouslyPairedFidoBleDeviceIds();
  ASSERT_TRUE(address_list_with_two_addresses);

  ASSERT_EQ(2u, address_list_with_two_addresses->GetSize());
  const auto& second_address_value =
      address_list_with_two_addresses->GetList().at(1);
  ASSERT_TRUE(second_address_value.is_string());
  EXPECT_EQ(kTestPairedDeviceAddress2, second_address_value.GetString());
}

#if defined(OS_MACOSX)
std::string TouchIdMetadataSecret(
    ChromeAuthenticatorRequestDelegate* delegate) {
  base::Optional<
      content::AuthenticatorRequestClientDelegate::TouchIdAuthenticatorConfig>
      config = delegate->GetTouchIdAuthenticatorConfig();
  return config->metadata_secret;
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, TouchIdMetadataSecret) {
  ChromeAuthenticatorRequestDelegate delegate(main_rfh(), kRelyingPartyID);
  std::string secret = TouchIdMetadataSecret(&delegate);
  EXPECT_EQ(secret.size(), 32u);
  EXPECT_EQ(secret, TouchIdMetadataSecret(&delegate));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_EqualForSameProfile) {
  // Different delegates on the same BrowserContext (Profile) should return the
  // same secret.
  ChromeAuthenticatorRequestDelegate delegate1(main_rfh(), kRelyingPartyID);
  ChromeAuthenticatorRequestDelegate delegate2(main_rfh(), kRelyingPartyID);
  EXPECT_EQ(TouchIdMetadataSecret(&delegate1),
            TouchIdMetadataSecret(&delegate2));
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_NotEqualForDifferentProfiles) {
  // Different profiles have different secrets. (No way to reset
  // browser_context(), so we have to create our own.)
  auto browser_context = base::WrapUnique(CreateBrowserContext());
  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context.get(), nullptr);
  ChromeAuthenticatorRequestDelegate delegate1(main_rfh(), kRelyingPartyID);
  ChromeAuthenticatorRequestDelegate delegate2(web_contents->GetMainFrame(),
                                               kRelyingPartyID);
  EXPECT_NE(TouchIdMetadataSecret(&delegate1),
            TouchIdMetadataSecret(&delegate2));
  // Ensure this second secret is actually valid.
  EXPECT_EQ(32u, TouchIdMetadataSecret(&delegate2).size());
}
#endif

#if defined(OS_WIN)
// Tests that ShouldReturnAttestation() returns with true if |authenticator|
// is the Windows native WebAuthn API with WEBAUTHN_API_VERSION_2 or higher,
// where Windows prompts for attestation in its own native UI.
//
// Ideally, this would also test the inverse case, i.e. that with
// WEBAUTHN_API_VERSION_1 Chrome's own attestation prompt is shown. However,
// there seems to be no good way to test AuthenticatorRequestDialogModel UI.
TEST_F(ChromeAuthenticatorRequestDelegateTest, ShouldPromptForAttestationWin) {
  ::device::ScopedFakeWinWebAuthnApi win_webauthn_api;
  win_webauthn_api.set_version(WEBAUTHN_API_VERSION_2);
  ::device::WinWebAuthnApiAuthenticator authenticator(
      /*current_window=*/nullptr);

  ::device::test::ValueCallbackReceiver<bool> cb;
  ChromeAuthenticatorRequestDelegate delegate(main_rfh(), kRelyingPartyID);
  delegate.ShouldReturnAttestation(kRelyingPartyID, &authenticator,
                                   cb.callback());
  cb.WaitForCallback();
  EXPECT_EQ(cb.value(), true);
}

// Ensures that DoesBlockOnRequestFailure() returns false if |authenticator|
// is the Windows native WebAuthn API because Chrome's request dialog UI
// should not show an error sheet after the user cancels out of the native
// Windows UI.
TEST_F(ChromeAuthenticatorRequestDelegateTest, DoesBlockRequestOnFailure) {
  ::device::ScopedFakeWinWebAuthnApi win_webauthn_api;
  ::device::WinWebAuthnApiAuthenticator win_authenticator(
      /*current_window=*/nullptr);
  ::device::FidoDeviceAuthenticator device_authenticator(/*device=*/nullptr);

  for (const bool use_win_api : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "use_win_api=" << use_win_api);

    ChromeAuthenticatorRequestDelegate delegate(main_rfh(), kRelyingPartyID);
    EXPECT_EQ(delegate.DoesBlockRequestOnFailure(
                  use_win_api ? static_cast<::device::FidoAuthenticator*>(
                                    &win_authenticator)
                              : static_cast<::device::FidoAuthenticator*>(
                                    &device_authenticator),
                  ChromeAuthenticatorRequestDelegate::InterestingFailureReason::
                      kUserConsentDenied),
              !use_win_api);
  }
}
#endif  // defined(OS_WIN)
