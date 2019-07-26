// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/payment_request.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/top_padding.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockActionDelegate : public ActionDelegate {
 public:
  MockActionDelegate();
  ~MockActionDelegate() override;

  MOCK_METHOD1(RunElementChecks, void(BatchElementChecker*));

  void ShortWaitForElement(const Selector& selector,
                           base::OnceCallback<void(bool)> callback) override {
    OnShortWaitForElement(selector, callback);
  }

  MOCK_METHOD2(OnShortWaitForElement,
               void(const Selector& selector, base::OnceCallback<void(bool)>&));

  void WaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      base::RepeatingCallback<void(BatchElementChecker*,
                                   base::OnceCallback<void(bool)>)>
          check_elements,
      base::OnceCallback<void(ProcessedActionStatusProto)> callback) override {
    OnWaitForDom(max_wait_time, allow_interrupt, check_elements, callback);
  }

  MOCK_METHOD4(
      OnWaitForDom,
      void(base::TimeDelta,
           bool,
           base::RepeatingCallback<void(BatchElementChecker*,
                                        base::OnceCallback<void(bool)>)>&,
           base::OnceCallback<void(ProcessedActionStatusProto)>&));

  MOCK_METHOD1(SetStatusMessage, void(const std::string& message));
  MOCK_METHOD0(GetStatusMessage, std::string());
  MOCK_METHOD1(SetBubbleMessage, void(const std::string& message));
  MOCK_METHOD0(GetBubbleMessage, std::string());
  MOCK_METHOD3(ClickOrTapElement,
               void(const Selector& selector,
                    ClickAction::ClickType click_type,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD1(Prompt,
               void(std::unique_ptr<std::vector<UserAction>> user_actions));
  MOCK_METHOD0(CancelPrompt, void());

  void FillAddressForm(
      const autofill::AutofillProfile* profile,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnFillAddressForm(profile, selector, callback);
  }

  MOCK_METHOD3(OnFillAddressForm,
               void(const autofill::AutofillProfile* profile,
                    const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  void FillCardForm(
      std::unique_ptr<autofill::CreditCard> card,
      const base::string16& cvc,
      const Selector& selector,
      base::OnceCallback<void(const ClientStatus&)> callback) override {
    OnFillCardForm(card->guid(), selector, callback);
  }

  MOCK_METHOD3(OnFillCardForm,
               void(const std::string& guid,
                    const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)>& callback));
  MOCK_METHOD3(SelectOption,
               void(const Selector& selector,
                    const std::string& selected_option,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD3(FocusElement,
               void(const Selector& selector,
                    const TopPadding& top_padding,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD1(SetTouchableElementArea,
               void(const ElementAreaProto& touchable_element_area));

  MOCK_METHOD2(HighlightElement,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD1(GetPaymentInformation,
               void(std::unique_ptr<PaymentRequestOptions> options));

  MOCK_METHOD1(GetFullCard, void(GetFullCardCallback callback));

  void GetFieldValue(
      const Selector& selector,
      base::OnceCallback<void(bool, const std::string&)> callback) {
    OnGetFieldValue(selector, callback);
  }

  MOCK_METHOD2(
      OnGetFieldValue,
      void(const Selector& selector,
           base::OnceCallback<void(bool, const std::string&)>& callback));

  void SetFieldValue(const Selector& selector,
                     const std::string& value,
                     bool ignored_simulate_key_presses,
                     int ignored_delay_in_millisecond,
                     base::OnceCallback<void(const ClientStatus&)> callback) {
    OnSetFieldValue(selector, value, callback);
  }

  MOCK_METHOD3(OnSetFieldValue,
               void(const Selector& selector,
                    const std::string& value,
                    base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD4(SetAttribute,
               void(const Selector& selector,
                    const std::vector<std::string>& attribute,
                    const std::string& value,
                    base::OnceCallback<void(const ClientStatus&)> callback));

  MOCK_METHOD4(SendKeyboardInput,
               void(const Selector& selector,
                    const std::vector<UChar32>& codepoints,
                    int delay_in_millisecond,
                    base::OnceCallback<void(const ClientStatus&)> callback));
  MOCK_METHOD2(GetOuterHtml,
               void(const Selector& selector,
                    base::OnceCallback<void(const ClientStatus&,
                                            const std::string&)> callback));
  MOCK_METHOD0(ExpectNavigation, void());
  MOCK_METHOD0(ExpectedNavigationHasStarted, bool());
  MOCK_METHOD1(WaitForNavigation,
               bool(base::OnceCallback<void(bool)> callback));
  MOCK_METHOD1(LoadURL, void(const GURL& url));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(Restart, void());
  MOCK_METHOD0(GetClientMemory, ClientMemory*());
  MOCK_METHOD0(GetPersonalDataManager, autofill::PersonalDataManager*());
  MOCK_METHOD0(GetWebContents, content::WebContents*());
  MOCK_METHOD1(SetDetails, void(std::unique_ptr<Details> details));
  MOCK_METHOD1(SetInfoBox, void(const InfoBox& info_box));
  MOCK_METHOD0(ClearInfoBox, void());
  MOCK_METHOD1(SetProgress, void(int progress));
  MOCK_METHOD1(SetProgressVisible, void(bool visible));
  MOCK_METHOD1(SetUserActions,
               void(std::unique_ptr<std::vector<UserAction>> user_action));
  MOCK_METHOD1(SetResizeViewport, void(bool resize_viewport));
  MOCK_METHOD0(GetResizeViewport, bool());
  MOCK_METHOD1(SetPeekMode,
               void(ConfigureBottomSheetProto::PeekMode peek_mode));
  MOCK_METHOD0(GetPeekMode, ConfigureBottomSheetProto::PeekMode());
  MOCK_METHOD2(
      SetForm,
      bool(std::unique_ptr<FormProto> form,
           base::RepeatingCallback<void(const FormProto::Result*)> callback));

  void WaitForWindowHeightChange(
      base::OnceCallback<void(const ClientStatus&)> callback) {
    OnWaitForWindowHeightChange(callback);
  }

  MOCK_METHOD1(OnWaitForWindowHeightChange,
               void(base::OnceCallback<void(const ClientStatus&)>& callback));

  MOCK_METHOD0(RequireUI, void());

  const ClientSettings& GetSettings() override { return client_settings_; }

  ClientSettings client_settings_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_MOCK_ACTION_DELEGATE_H_
