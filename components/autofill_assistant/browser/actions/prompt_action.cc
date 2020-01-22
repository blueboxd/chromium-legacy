// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/element_precondition.h"
#include "url/gurl.h"

namespace autofill_assistant {

PromptAction::PromptAction(ActionDelegate* delegate, const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_prompt());
}

PromptAction::~PromptAction() {}

void PromptAction::InternalProcessAction(ProcessActionCallback callback) {
  callback_ = std::move(callback);
  if (proto_.prompt().choices_size() == 0) {
    EndAction(INVALID_ACTION);
    return;
  }

  if (proto_.prompt().has_message()) {
    // TODO(b/144468818): Deprecate and remove message from this action and use
    // tell instead.
    delegate_->SetStatusMessage(proto_.prompt().message());
  }

  SetupPreconditions();
  UpdateUserActions();

  if (HasNonemptyPreconditions() || HasAutoSelect() ||
      proto_.prompt().allow_interrupt()) {
    delegate_->WaitForDom(base::TimeDelta::Max(),
                          proto_.prompt().allow_interrupt(),
                          base::BindRepeating(&PromptAction::RegisterChecks,
                                              weak_ptr_factory_.GetWeakPtr()),
                          base::BindOnce(&PromptAction::OnDoneWaitForDom,
                                         weak_ptr_factory_.GetWeakPtr()));
  }
}

void PromptAction::RegisterChecks(
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  if (!callback_) {
    // Action is done; checks aren't necessary anymore.
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }

  UpdateUserActions();

  for (size_t i = 0; i < preconditions_.size(); i++) {
    preconditions_[i]->Check(checker,
                             base::BindOnce(&PromptAction::OnPreconditionResult,
                                            weak_ptr_factory_.GetWeakPtr(), i));
  }

  auto_select_choice_index_ = -1;
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    const PromptProto::Choice& choice = proto_.prompt().choices(i);
    switch (choice.auto_select_case()) {
      case PromptProto::Choice::kAutoSelectIfElementExists:
        checker->AddElementCheck(
            Selector(choice.auto_select_if_element_exists()),
            base::BindOnce(&PromptAction::OnAutoSelectElementExists,
                           weak_ptr_factory_.GetWeakPtr(), i,
                           /* must_exist= */ true));
        break;

      case PromptProto::Choice::kAutoSelectIfElementDisappears:
        checker->AddElementCheck(
            Selector(choice.auto_select_if_element_disappears()),
            base::BindOnce(&PromptAction::OnAutoSelectElementExists,
                           weak_ptr_factory_.GetWeakPtr(), i,
                           /* must_exist= */ false));
        break;

      case PromptProto::Choice::AUTO_SELECT_NOT_SET:
        break;
    }
  }

  checker->AddAllDoneCallback(base::BindOnce(&PromptAction::OnElementChecksDone,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             std::move(wait_for_dom_callback)));
}

void PromptAction::SetupPreconditions() {
  int choice_count = proto_.prompt().choices_size();
  preconditions_.resize(choice_count);
  precondition_results_.resize(choice_count);
  for (int i = 0; i < choice_count; i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    preconditions_[i] = std::make_unique<ElementPrecondition>(
        choice_proto.show_only_if_element_exists(),
        choice_proto.show_only_if_form_value_matches());
    precondition_results_[i] = preconditions_[i]->empty();
  }
}

bool PromptAction::HasNonemptyPreconditions() {
  for (const auto& precondition : preconditions_) {
    if (!precondition->empty())
      return true;
  }
  return false;
}

void PromptAction::OnPreconditionResult(size_t choice_index, bool result) {
  if (precondition_results_[choice_index] == result)
    return;

  precondition_results_[choice_index] = result;
  precondition_changed_ = true;
}

void PromptAction::UpdateUserActions() {
  DCHECK(callback_);  // Make sure we're still waiting for a response

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    UserAction user_action(choice_proto.chip(), choice_proto.direct_action());
    if (!user_action.has_triggers())
      continue;

    // Hide actions whose preconditions don't match.
    if (!precondition_results_[i] && !choice_proto.allow_disabling())
      continue;

    user_action.SetEnabled(precondition_results_[i]);
    user_action.SetCallback(base::BindOnce(&PromptAction::OnSuggestionChosen,
                                           weak_ptr_factory_.GetWeakPtr(), i));
    user_actions->emplace_back(std::move(user_action));
  }
  delegate_->Prompt(std::move(user_actions));
  precondition_changed_ = false;
}

bool PromptAction::HasAutoSelect() {
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    if (proto_.prompt().choices(i).auto_select_case() !=
        PromptProto::Choice::AUTO_SELECT_NOT_SET) {
      return true;
    }
  }
  return false;
}

void PromptAction::OnAutoSelectElementExists(
    int choice_index,
    bool must_exist,
    const ClientStatus& element_status) {
  if ((must_exist && element_status.ok()) ||
      (!must_exist &&
       element_status.proto_status() == ELEMENT_RESOLUTION_FAILED)) {
    auto_select_choice_index_ = choice_index;
  }

  // Calling OnSuggestionChosen() is delayed until try_done, as it indirectly
  // deletes the batch element checker, which isn't supported from an element
  // check callback.
}

void PromptAction::OnElementChecksDone(
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  if (precondition_changed_)
    UpdateUserActions();

  // Calling wait_for_dom_callback with successful status is a way of asking the
  // WaitForDom to end gracefully and call OnDoneWaitForDom with the status.
  // Note that it is possible for WaitForDom to decide not to call
  // OnDoneWaitForDom, if an interrupt triggers at the same time, so we cannot
  // cancel the prompt and choose the suggestion just yet.
  if (auto_select_choice_index_ >= 0) {
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }

  // Let WaitForDom know we're still waiting for an element.
  std::move(wait_for_dom_callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED));
}

void PromptAction::OnDoneWaitForDom(const ClientStatus& status) {
  // Status doesn't matter; it's just forwarded from AutoSelectDone.
  if (auto_select_choice_index_ >= 0) {
    OnSuggestionChosen(auto_select_choice_index_);
  }
}

void PromptAction::OnSuggestionChosen(int choice_index) {
  if (!callback_) {
    NOTREACHED();
    return;
  }
  DCHECK(choice_index >= 0 && choice_index <= proto_.prompt().choices_size());

  PromptProto::Choice choice;
  *processed_action_proto_->mutable_prompt_choice() =
      proto_.prompt().choices(choice_index);
  EndAction(ACTION_APPLIED);
}

void PromptAction::EndAction(const ProcessedActionStatusProto& status) {
  delegate_->CleanUpAfterPrompt();
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
