// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_ELIGIBILITY_CHECKER_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_ELIGIBILITY_CHECKER_H_

#include <string>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"

namespace chromeos {

namespace sync_wifi {

bool IsEligibleForSync(const std::string& guid,
                       bool is_connectable,
                       const network_config::mojom::OncSource& onc_source,
                       const network_config::mojom::SecurityType& security_type,
                       bool log_result);

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_NETWORK_ELIGIBILITY_CHECKER_H_
