// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASIC_INTERACTIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASIC_INTERACTIONS_H_

#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"

namespace autofill_assistant {
class ScriptExecutorDelegate;
class UserModel;

// Provides basic interactions for use by the generic UI framework. These
// methods are intended to be bound to by the corresponding interaction
// handlers.
class BasicInteractions {
 public:
  // Constructor. |delegate| must outlive this instance.
  BasicInteractions(ScriptExecutorDelegate* delegate);
  ~BasicInteractions();

  BasicInteractions(const BasicInteractions&) = delete;
  BasicInteractions& operator=(const BasicInteractions&) = delete;

  base::WeakPtr<BasicInteractions> GetWeakPtr();

  // Performs the computation specified by |proto| and writes the result to
  // |user_model_|. Returns true on success, false on error.
  bool ComputeValue(const ComputeValueProto& proto);

  // Sets a value in |user_model_| as specified by |proto|. Returns true on
  // success, false on error.
  bool SetValue(const SetModelValueProto& proto);

  // Replaces the set of available user actions as specified by |proto|. Returns
  // true on success, false on error.
  bool SetUserActions(const SetUserActionsProto& proto);

  // Enables or disables a user action. Returns true on success, false on error.
  bool ToggleUserAction(const ToggleUserActionProto& proto);

  // Ends the current action. Can only be called during a ShowGenericUiAction.
  bool EndAction(bool view_inflation_successful, const EndActionProto& proto);

  // Sets the callback to end the current ShowGenericUiAction.
  void SetEndActionCallback(
      base::OnceCallback<void(bool,
                              ProcessedActionStatusProto,
                              const UserModel*)> end_action_callback);

  // Clears the |end_action_callback_|.
  void ClearEndActionCallback();

  // Runs |callback| if |condition_identifier| points to a single boolean set to
  // 'true'. Returns true on success (i.e., condition was evaluated
  // successfully), false on failure.
  bool RunConditionalCallback(const std::string& condition_identifier,
                              base::RepeatingCallback<void()> callback);

  // Disables all radio buttons in |model_identifiers| except
  // |selected_model_identifier|. Fails if one or more model identifiers are not
  // found.
  bool UpdateRadioButtonGroup(const std::vector<std::string>& model_identifiers,
                              const std::string& selected_model_identifier);

 private:
  ScriptExecutorDelegate* delegate_;
  // Only valid during a ShowGenericUiAction.
  base::OnceCallback<void(bool, ProcessedActionStatusProto, const UserModel*)>
      end_action_callback_;
  base::WeakPtrFactory<BasicInteractions> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASIC_INTERACTIONS_H_
