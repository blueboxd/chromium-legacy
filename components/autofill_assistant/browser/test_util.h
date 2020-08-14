// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TEST_UTIL_H_

#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// This comparison operator is only suitable for comparing protos created in
// tests, not for comparing protos received externally.
bool operator==(const google::protobuf::MessageLite& proto_a,
                const google::protobuf::MessageLite& proto_b);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TEST_UTIL_H_
