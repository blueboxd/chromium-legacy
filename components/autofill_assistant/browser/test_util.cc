// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/test_util.h"

namespace autofill_assistant {

bool operator==(const google::protobuf::MessageLite& proto_a,
                const google::protobuf::MessageLite& proto_b) {
  std::string serialized_proto_a;
  proto_a.SerializeToString(&serialized_proto_a);

  std::string serialized_proto_b;
  proto_b.SerializeToString(&serialized_proto_b);

  return serialized_proto_a == serialized_proto_b;
}

}  // namespace autofill_assistant
