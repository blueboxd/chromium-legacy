// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/ui_controller.h"

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill_assistant {

UiController::UiController() = default;
UiController::~UiController() = default;

void UiController::OnStateChanged(AutofillAssistantState new_state) {}
void UiController::OnStatusMessageChanged(const std::string& message) {}
void UiController::WillShutdown(Metrics::DropOutReason reason) {}
void UiController::OnSuggestionsChanged(const std::vector<Chip>& suggestions) {}
void UiController::OnActionsChanged(const std::vector<Chip>& actions) {}
void UiController::OnPaymentRequestOptionsChanged(
    const PaymentRequestOptions* options) {}
void UiController::OnPaymentRequestInformationChanged(
    const PaymentInformation* state) {}
void UiController::OnDetailsChanged(const Details* details) {}
void UiController::OnInfoBoxChanged(const InfoBox* info_box) {}
void UiController::OnProgressChanged(int progress) {}
void UiController::OnProgressVisibilityChanged(bool visible) {}
void UiController::OnTouchableAreaChanged(
    const RectF& visual_viewport,
    const std::vector<RectF>& touchable_areas,
    const std::vector<RectF>& restricted_areas) {}
void UiController::OnResizeViewportChanged(bool resize_viewport) {}
void UiController::OnPeekModeChanged(
    ConfigureBottomSheetProto::PeekMode peek_mode) {}
void UiController::OnOverlayColorsChanged(
    const UiDelegate::OverlayColors& colors) {}
void UiController::OnFormChanged(const FormProto* form) {}

}  // namespace autofill_assistant
