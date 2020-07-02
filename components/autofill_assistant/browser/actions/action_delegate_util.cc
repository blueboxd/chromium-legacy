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

}  // namespace ActionDelegateUtil
}  // namespace autofill_assistant
