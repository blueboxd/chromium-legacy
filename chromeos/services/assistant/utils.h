// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_UTILS_H_
#define CHROMEOS_SERVICES_ASSISTANT_UTILS_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace assistant {

base::FilePath GetRootPath();

// Creates the configuration for libassistant.
std::string CreateLibAssistantConfig(
    base::Optional<std::string> s3_server_uri_override = base::nullopt);

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_UTILS_H_
