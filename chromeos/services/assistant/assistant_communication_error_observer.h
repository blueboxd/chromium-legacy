// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_COMMUNICATION_ERROR_OBSERVER_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_COMMUNICATION_ERROR_OBSERVER_H_

#include "base/macros.h"
#include "base/observer_list_types.h"

namespace chromeos {
namespace assistant {

enum class CommunicationErrorType {
  AuthenticationError,
  Other,
};

// A checked observer that observes communication errors when communicating with
// the Assistant backend.
class AssistantCommunicationErrorObserver : public base::CheckedObserver {
 public:
  AssistantCommunicationErrorObserver() = default;

  virtual void OnCommunicationError(CommunicationErrorType error) = 0;

 protected:
  ~AssistantCommunicationErrorObserver() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantCommunicationErrorObserver);
};
}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_COMMUNICATION_ERROR_OBSERVER_H_
