// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_
#define GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_

#include <ostream>
#include <string>
#include <vector>

#include "build/build_config.h"

// Represent the id of an account for interaction with GAIA. It is
// currently implicitly convertible to and from std::string to allow
// progressive migration of the code (see https://crbug.com/959157
// for design and tracking).
struct CoreAccountId {
  CoreAccountId();
  CoreAccountId(const CoreAccountId&);
  CoreAccountId(CoreAccountId&&) noexcept;
  ~CoreAccountId();

  CoreAccountId& operator=(const CoreAccountId&);
  CoreAccountId& operator=(CoreAccountId&&) noexcept;

  explicit CoreAccountId(const char* id);
  explicit CoreAccountId(std::string&& id);
  explicit CoreAccountId(const std::string& id);

  // Checks if the account is valid or not.
  bool empty() const;

  std::string id;
};

bool operator<(const CoreAccountId& lhs, const CoreAccountId& rhs);

bool operator==(const CoreAccountId& lhs, const CoreAccountId& rhs);

bool operator!=(const CoreAccountId& lhs, const CoreAccountId& rhs);

std::ostream& operator<<(std::ostream& out, const CoreAccountId& a);

// Returns the values of the account ids in a vector. Useful especially for
// logs.
std::vector<std::string> ToStringList(
    const std::vector<CoreAccountId>& account_ids);

namespace std {
template <>
struct hash<CoreAccountId> {
  size_t operator()(const CoreAccountId& account_id) const {
    return std::hash<std::string>()(account_id.id);
  }
};
}  // namespace std

#endif  // GOOGLE_APIS_GAIA_CORE_ACCOUNT_ID_H_
