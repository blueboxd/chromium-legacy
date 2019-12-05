// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_PERMISSION_H_
#define COMPONENTS_EXO_PERMISSION_H_

#include "base/time/time.h"

namespace exo {

class Permission {
 public:
  enum class Capability {
    kActivate,
  };

  // Create a permission with the given |capability| until |timeout| elapses.
  Permission(Capability capability, base::TimeDelta timeout);

  // Delete copy and move.
  Permission(const Permission& other) = delete;
  Permission(Permission&& other) = delete;
  Permission& operator=(const Permission& other) = delete;
  Permission& operator=(Permission&& other) = delete;

  virtual ~Permission() = default;

  // Prevent this permission from returning true on subsequent Check()s.
  void Revoke();

  // Returns true iff this permission was created with the given |capability|
  // and is not expired.
  bool Check(Capability capability) const;

 private:
  Capability capability_;

  base::Time expiry_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_PERMISSION_H_
