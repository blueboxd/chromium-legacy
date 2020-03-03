// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASIC_INTERACTIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASIC_INTERACTIONS_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/interactions.pb.h"

namespace autofill_assistant {
class ScriptExecutorDelegate;

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

 private:
  ScriptExecutorDelegate* delegate_;
  base::WeakPtrFactory<BasicInteractions> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BASIC_INTERACTIONS_H_
