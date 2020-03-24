// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_ERROR_MESSAGE_UTIL_H_
#define COMPONENTS_PAYMENTS_CORE_ERROR_MESSAGE_UTIL_H_

#include <set>
#include <string>

namespace payments {

// Returns a developer-facing error message that the given payment |methods| are
// not supported.
std::string GetNotSupportedErrorMessage(const std::set<std::string>& methods);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_ERROR_MESSAGE_UTIL_H_
