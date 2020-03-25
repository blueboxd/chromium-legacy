// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_change.h"

namespace password_manager {

std::ostream& operator<<(std::ostream& os,
                         const PasswordStoreChange& password_store_change) {
  return os << "type: " << password_store_change.type()
            << ", primary key: " << password_store_change.primary_key()
            << ", password change: " << password_store_change.password_changed()
            << ", password form: " << password_store_change.form();
}

}  // namespace password_manager
