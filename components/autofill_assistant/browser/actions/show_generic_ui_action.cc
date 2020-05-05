// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_generic_ui_action.h"

#include <utility>
#include "base/optional.h"

#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

ShowGenericUiAction::ShowGenericUiAction(ActionDelegate* delegate,
                                         const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_show_generic_ui());
}

ShowGenericUiAction::~ShowGenericUiAction() {}

void ShowGenericUiAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  // Check that |output_model_identifiers| is a subset of input model.
  UserModel temp_model;
  temp_model.MergeWithProto(
      proto_.show_generic_ui().generic_user_interface().model(),
      /* force_notifications = */ false);
  if (!temp_model.GetValues(proto_.show_generic_ui().output_model_identifiers())
           .has_value()) {
    EndAction(false, INVALID_ACTION, nullptr);
    return;
  }

  delegate_->Prompt(/* user_actions = */ nullptr,
                    /* disable_force_expand_sheet = */ false);
  delegate_->SetGenericUi(
      std::make_unique<GenericUserInterfaceProto>(
          proto_.show_generic_ui().generic_user_interface()),
      base::BindOnce(&ShowGenericUiAction::EndAction,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShowGenericUiAction::EndAction(bool view_inflation_successful,
                                    ProcessedActionStatusProto status,
                                    const UserModel* user_model) {
  delegate_->ClearGenericUi();
  delegate_->CleanUpAfterPrompt();
  UpdateProcessedAction(status);
  if (view_inflation_successful) {
    DCHECK(user_model);
    const auto& output_model_identifiers =
        proto_.show_generic_ui().output_model_identifiers();
    auto values = user_model->GetValues(output_model_identifiers);
    // This should always be the case since there is no way to erase a value
    // from the model.
    DCHECK(values.has_value());
    auto* output_model =
        processed_action_proto_->mutable_show_generic_ui_result()
            ->mutable_model();
    for (size_t i = 0; i < values->size(); ++i) {
      auto* output_value = output_model->add_values();
      output_value->set_identifier(output_model_identifiers.at(i));
      if (!values->at(i).is_client_side_only()) {
        *output_value->mutable_value() = values->at(i);
      }
    }
  }

  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
