// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/controller_observer.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill_assistant {

ControllerObserver::ControllerObserver() = default;
ControllerObserver::~ControllerObserver() = default;

void ControllerObserver::OnStateChanged(AutofillAssistantState new_state) {}
void ControllerObserver::OnStatusMessageChanged(const std::string& message) {}
void ControllerObserver::OnBubbleMessageChanged(const std::string& message) {}
void ControllerObserver::CloseCustomTab() {}
void ControllerObserver::OnUserActionsChanged(
    const std::vector<UserAction>& user_actions) {}
void ControllerObserver::OnPaymentRequestOptionsChanged(
    const PaymentRequestOptions* options) {}
void ControllerObserver::OnPaymentRequestInformationChanged(
    const PaymentInformation* state) {}
void ControllerObserver::OnDetailsChanged(const Details* details) {}
void ControllerObserver::OnInfoBoxChanged(const InfoBox* info_box) {}
void ControllerObserver::OnProgressChanged(int progress) {}
void ControllerObserver::OnProgressVisibilityChanged(bool visible) {}
void ControllerObserver::OnTouchableAreaChanged(
    const RectF& visual_viewport,
    const std::vector<RectF>& touchable_areas,
    const std::vector<RectF>& restricted_areas) {}
void ControllerObserver::OnViewportModeChanged(ViewportMode mode) {}
void ControllerObserver::OnPeekModeChanged(
    ConfigureBottomSheetProto::PeekMode peek_mode) {}
void ControllerObserver::OnOverlayColorsChanged(
    const UiDelegate::OverlayColors& colors) {}
void ControllerObserver::OnFormChanged(const FormProto* form) {}

}  // namespace autofill_assistant
