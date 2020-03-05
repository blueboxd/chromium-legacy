// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/basic_interactions.h"
#include "base/bind_helpers.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

namespace {

bool BooleanAnd(UserModel* user_model,
                const std::string& result_model_identifier,
                const BooleanAndProto& proto) {
  auto values = user_model->GetValues(proto.model_identifiers());
  if (!values.has_value()) {
    DVLOG(2) << "Failed to find values in user model";
    return false;
  }

  if (!AreAllValuesOfType(*values, ValueProto::kBooleans) ||
      !AreAllValuesOfSize(*values, 1)) {
    DVLOG(2) << "All values must be 'boolean' and contain exactly 1 value each";
    return false;
  }

  bool result = true;
  for (const auto& value : *values) {
    result &= value.booleans().values(0);
  }
  user_model->SetValue(result_model_identifier, SimpleValue(result));
  return true;
}

bool BooleanOr(UserModel* user_model,
               const std::string& result_model_identifier,
               const BooleanOrProto& proto) {
  auto values = user_model->GetValues(proto.model_identifiers());
  if (!values.has_value()) {
    DVLOG(2) << "Failed to find values in user model";
    return false;
  }

  if (!AreAllValuesOfType(*values, ValueProto::kBooleans) ||
      !AreAllValuesOfSize(*values, 1)) {
    DVLOG(2) << "All values must be 'boolean' and contain exactly 1 value each";
    return false;
  }

  bool result = false;
  for (const auto& value : *values) {
    result |= value.booleans().values(0);
  }
  user_model->SetValue(result_model_identifier, SimpleValue(result));
  return true;
}

bool BooleanNot(UserModel* user_model,
                const std::string& result_model_identifier,
                const BooleanNotProto& proto) {
  auto value = user_model->GetValue(proto.model_identifier());
  if (!value.has_value()) {
    DVLOG(2) << "Error evaluating " << __func__ << ": "
             << proto.model_identifier() << " not found in model";
    return false;
  }
  if (value->booleans().values().size() != 1) {
    DVLOG(2) << "Error evaluating " << __func__
             << ": expected single boolean, but got " << *value;
    return false;
  }

  user_model->SetValue(result_model_identifier,
                       SimpleValue(!value->booleans().values(0)));
  return true;
}

}  // namespace

base::WeakPtr<BasicInteractions> BasicInteractions::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BasicInteractions::BasicInteractions(ScriptExecutorDelegate* delegate)
    : delegate_(delegate) {}

BasicInteractions::~BasicInteractions() {}

bool BasicInteractions::SetValue(const SetModelValueProto& proto) {
  if (proto.model_identifier().empty()) {
    DVLOG(2) << "Error setting value: model_identifier empty";
    return false;
  }
  delegate_->GetUserModel()->SetValue(proto.model_identifier(), proto.value());
  return true;
}

bool BasicInteractions::ComputeValue(const ComputeValueProto& proto) {
  if (proto.result_model_identifier().empty()) {
    DVLOG(2) << "Error computing value: result_model_identifier empty";
    return false;
  }

  switch (proto.kind_case()) {
    case ComputeValueProto::kBooleanAnd:
      if (proto.boolean_and().model_identifiers().size() == 0) {
        DVLOG(2) << "Error computing ComputeValue::BooleanAnd: no "
                    "model_identifiers specified";
        return false;
      }
      return BooleanAnd(delegate_->GetUserModel(),
                        proto.result_model_identifier(), proto.boolean_and());
    case ComputeValueProto::kBooleanOr:
      if (proto.boolean_or().model_identifiers().size() == 0) {
        DVLOG(2) << "Error computing ComputeValue::BooleanOr: no "
                    "model_identifiers specified";
        return false;
      }
      return BooleanOr(delegate_->GetUserModel(),
                       proto.result_model_identifier(), proto.boolean_or());
    case ComputeValueProto::kBooleanNot:
      if (proto.boolean_not().model_identifier().empty()) {
        DVLOG(2) << "Error computing ComputeValue::BooleanNot: "
                    "model_identifier not specified";
        return false;
      }
      return BooleanNot(delegate_->GetUserModel(),
                        proto.result_model_identifier(), proto.boolean_not());
    case ComputeValueProto::KIND_NOT_SET:
      DVLOG(2) << "Error computing value: kind not set";
      return false;
  }
}

bool BasicInteractions::SetUserActions(const SetUserActionsProto& proto) {
  if (proto.model_identifier().empty()) {
    DVLOG(2) << "Error setting user actions: model_identifier empty";
    return false;
  }
  auto user_actions_value =
      delegate_->GetUserModel()->GetValue(proto.model_identifier());
  if (!user_actions_value.has_value()) {
    DVLOG(2) << "Error setting user actions: " << proto.model_identifier()
             << " not found in model";
    return false;
  }
  if (!user_actions_value->has_user_actions()) {
    DVLOG(2) << "Error setting user actions: Expected "
             << proto.model_identifier() << " to hold UserActions, but found "
             << user_actions_value->kind_case() << " instead";
    return false;
  }

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  for (const auto& user_action : user_actions_value->user_actions().values()) {
    user_actions->push_back({user_action});
    // No callback needed, the framework relies on generic events which will
    // be fired automatically when user actions are called.
    user_actions->back().SetCallback(
        base::DoNothing::Once<std::unique_ptr<TriggerContext>>());
  }

  delegate_->SetUserActions(std::move(user_actions));
  return true;
}

bool BasicInteractions::EndAction(const EndActionProto& proto) {
  CHECK(end_action_callback_) << "Failed to EndAction: no callback set";
  std::move(end_action_callback_)
      .Run(proto.status(), delegate_->GetUserModel());
  return true;
}

void BasicInteractions::ClearEndActionCallback() {
  end_action_callback_.Reset();
}

void BasicInteractions::SetEndActionCallback(
    base::OnceCallback<void(ProcessedActionStatusProto, const UserModel*)>
        end_action_callback) {
  end_action_callback_ = std::move(end_action_callback);
}

}  // namespace autofill_assistant
