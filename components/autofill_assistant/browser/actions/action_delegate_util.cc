// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/action_delegate_util.h"

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder.h"

namespace autofill_assistant {
namespace ActionDelegateUtil {
namespace {

void RetainElementAndExecuteCallback(
    std::unique_ptr<ElementFinder::Result> element,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& status) {
  DCHECK(element != nullptr);
  std::move(callback).Run(status);
}

void OnFindElementForClickOrTap(
    ActionDelegate* delegate,
    ClickType click_type,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element) {
  if (!element_status.ok()) {
    VLOG(1) << __func__ << " Failed to find the element to click or tap.";
    std::move(callback).Run(element_status);
    return;
  }

  delegate->ClickOrTapElement(
      *element, click_type,
      base::BindOnce(&RetainElementAndExecuteCallback, std::move(element),
                     std::move(callback)));
}

void OnFindElementForSendKeyboardInput(
    ActionDelegate* delegate,
    const std::vector<UChar32> codepoints,
    int delay_in_millis,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element) {
  if (!element_status.ok()) {
    VLOG(1) << __func__
            << " Failed to find the element to send keyboad input to.";
    std::move(callback).Run(element_status);
    return;
  }

  delegate->SendKeyboardInput(
      *element, codepoints, delay_in_millis,
      base::BindOnce(&RetainElementAndExecuteCallback, std::move(element),
                     std::move(callback)));
}

void OnFindElementForSetFieldValue(
    ActionDelegate* delegate,
    const std::string& value,
    KeyboardValueFillStrategy fill_strategy,
    int key_press_delay_in_millisecond,
    base::OnceCallback<void(const ClientStatus&)> callback,
    const ClientStatus& element_status,
    std::unique_ptr<ElementFinder::Result> element) {
  if (!element_status.ok()) {
    VLOG(1) << __func__ << " Failed to find element to set value.";
    std::move(callback).Run(element_status);
    return;
  }

  delegate->SetFieldValue(
      *element, value, fill_strategy, key_press_delay_in_millisecond,
      base::BindOnce(&RetainElementAndExecuteCallback, std::move(element),
                     std::move(callback)));
}

}  // namespace

void ClickOrTapElement(ActionDelegate* delegate,
                       const Selector& selector,
                       ClickType click_type,
                       base::OnceCallback<void(const ClientStatus&)> callback) {
  VLOG(3) << __func__ << " " << selector;
  DCHECK(!selector.empty());
  delegate->FindElement(selector,
                        base::BindOnce(&OnFindElementForClickOrTap, delegate,
                                       click_type, std::move(callback)));
}

void SendKeyboardInput(ActionDelegate* delegate,
                       const Selector& selector,
                       const std::vector<UChar32> codepoints,
                       int delay_in_millis,
                       base::OnceCallback<void(const ClientStatus&)> callback) {
  VLOG(3) << __func__ << " " << selector;
  DCHECK(!selector.empty());
  delegate->FindElement(
      selector,
      base::BindOnce(&OnFindElementForSendKeyboardInput, delegate, codepoints,
                     delay_in_millis, std::move(callback)));
}

void SetFieldValue(ActionDelegate* delegate,
                   const Selector& selector,
                   const std::string& value,
                   KeyboardValueFillStrategy fill_strategy,
                   int key_press_delay_in_millisecond,
                   base::OnceCallback<void(const ClientStatus&)> callback) {
  VLOG(3) << __func__ << " " << selector;
  DCHECK(!selector.empty());
  delegate->FindElement(
      selector, base::BindOnce(&OnFindElementForSetFieldValue, delegate, value,
                               fill_strategy, key_press_delay_in_millisecond,
                               std::move(callback)));
}

}  // namespace ActionDelegateUtil
}  // namespace autofill_assistant
