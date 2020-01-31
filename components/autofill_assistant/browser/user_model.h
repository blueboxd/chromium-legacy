// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Manages a map of |ValueProto| instances and notifies observers of changes.
//
// - It is safe to add/remove observers at any time.
// - Provides a == comparison operator for |ValueProto|.
// - Provides a << output operator for |ValueProto| for debugging.
class UserModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer();
    ~Observer() override;

    virtual void OnValueChanged(const std::string& identifier,
                                const ValueProto& new_value) = 0;
  };

  UserModel();
  ~UserModel();

  base::WeakPtr<UserModel> GetWeakPtr();

  // Writes |value| to |identifier|, potentially overwriting the previously
  // stored value. If the new value is different or |force_notification| is
  // true, a change notification will be fired.
  void SetValue(const std::string& identifier,
                const ValueProto& value,
                bool force_notification = false);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Merges *this with |another| such that the result is the union of both.
  // In case of ambiguity, |another| takes precedence. Empty values in
  // |another| do not overwrite non-empty values in *this.
  // If |force_notifications| is true, a value-changed notification will be
  // fired for every value in |another|, even if the value has not changed.
  void MergeWithProto(const ModelProto& another, bool force_notifications);

  // Updates the current values of all identifiers contained in |model_proto|.
  void UpdateProto(ModelProto* model_proto) const;

 private:
  friend class UserModelTest;

  std::map<std::string, ValueProto> values_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<UserModel> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(UserModel);
};

// Custom comparison operator for |ValueProto|, because we can't use
// |MessageDifferencer| for protobuf lite and can't rely on serialization.
bool operator==(const ValueProto& value_a, const ValueProto& value_b);

// Custom comparison operator for |ModelValue|.
bool operator==(const ModelProto::ModelValue& value_a,
                const ModelProto::ModelValue& value_b);

// Intended for debugging.
std::ostream& operator<<(std::ostream& out, const ValueProto& value);

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_USER_MODEL_H_
