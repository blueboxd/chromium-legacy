// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_

#include <map>
#include <string>

#include "base/values.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class TriggerContext;

class Details {
 public:
  const DetailsProto& detailsProto() const { return proto_; }
  const DetailsChangesProto& changes() const { return change_flags_; }
  const std::string GetDatetime() const { return datetime_; }

  // Returns a dictionary describing the current execution context, which
  // is intended to be serialized as JSON string. The execution context is
  // useful when analyzing feedback forms and for debugging in general.
  base::Value GetDebugContext() const;

  // Update details from the given parameters. Returns true if changes were
  // made.
  // If one of the generic detail parameter is present then vertical specific
  // parameters are not used for Details creation.
  bool UpdateFromParameters(const TriggerContext& context);

  // Updates the details to show data directly from proto. Returns true if
  // |details| were successfully updated.
  static bool UpdateFromProto(const ShowDetailsProto& proto, Details* details);

  // Updates the details to show selected contact details. It shows only full
  // name and email. Returns true if |details| were successfully updated.
  static bool UpdateFromContactDetails(const ShowDetailsProto& proto,
                                       ClientMemory* client_memory,
                                       Details* details);

  // Updates the details to show selected shipping details. It shows full name
  // and address. Returns true if |details| were successfully updated.
  static bool UpdateFromShippingAddress(const ShowDetailsProto& proto,
                                        ClientMemory* client_memory,
                                        Details* details);

  // Updates the details to show credit card selected by the user. Returns true
  // if |details| were successfully updated.
  static bool UpdateFromSelectedCreditCard(const ShowDetailsProto& proto,
                                           ClientMemory* client_memory,
                                           Details* details);

  void SetDetailsProto(const DetailsProto& proto) { proto_ = proto; }
  void SetDetailsChangesProto(const DetailsChangesProto& change_flags) {
    change_flags_ = change_flags;
  }
  // Clears all change flags.
  void ClearChanges();

 private:
  // Tries updating the details using generic detail parameters. Returns true
  // if at least one generic detail parameter was found and used.
  bool MaybeUpdateFromDetailsParameters(const TriggerContext& context);

  DetailsProto proto_;
  DetailsChangesProto change_flags_;

  // RFC 3339 date-time. Ignore if proto.description_line_1 is set.
  //
  // TODO(crbug.com/806868): parse RFC 3339 date-time on the C++ side and fill
  // proto.description_line_1 with the result instead of carrying a string
  // representation of the datetime.
  std::string datetime_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_
