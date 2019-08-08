// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_

#include <string>

namespace password_manager {

// Produces the username/password pair hash using scrypt algorithm.
// |username| and |password| are UTF-8 strings.
std::string ScryptHashUsernameAndPassword(const std::string& username,
                                          const std::string& password);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_ENCRYPTION_UTILS_H_
