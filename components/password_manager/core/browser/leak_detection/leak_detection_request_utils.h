// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_

#include "base/callback.h"
#include "base/strings/string_piece_forward.h"

namespace google {
namespace internal {
namespace identity {
namespace passwords {
namespace leak {
namespace check {
namespace v1 {
class LookupSingleLeakRequest;
}  // namespace v1
}  // namespace check
}  // namespace leak
}  // namespace passwords
}  // namespace identity
}  // namespace internal
}  // namespace google

namespace password_manager {

struct SingleLookupResponse;

// Stores all the data needed for one credential lookup.
struct LookupSingleLeakData {
  LookupSingleLeakData() = default;
  LookupSingleLeakData(LookupSingleLeakData&& other) = default;
  LookupSingleLeakData& operator=(LookupSingleLeakData&& other) = default;
  ~LookupSingleLeakData() = default;

  LookupSingleLeakData(const LookupSingleLeakData&) = delete;
  LookupSingleLeakData& operator=(const LookupSingleLeakData&) = delete;

  std::string username_hash_prefix;
  std::string encrypted_payload;

  std::string encryption_key;
};

using SingleLeakRequestDataCallback =
    base::OnceCallback<void(LookupSingleLeakData)>;

// Constructs a LookupSingleLeakRequest from the provided |username| and
// |password|.
google::internal::identity::passwords::leak::check::v1::LookupSingleLeakRequest
MakeLookupSingleLeakRequest(base::StringPiece username,
                            base::StringPiece password);

// Asynchronously creates a data payload for single credential check.
// Callback is invoked on the calling thread with the protobuf and the
// encryption key used.
void PrepareSingleLeakRequestData(const std::string& username,
                                  const std::string& password,
                                  SingleLeakRequestDataCallback callback);

// Processes the provided |response| and returns whether the relevant credential
// was leaked.
bool ParseLookupSingleLeakResponse(const SingleLookupResponse& response);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_
