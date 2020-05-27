// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/network_eligibility_checker.h"

#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom.h"

namespace chromeos {

namespace sync_wifi {

bool IsEligibleForSync(const std::string& guid,
                       bool is_connectable,
                       const network_config::mojom::SecurityType& security_type,
                       bool log_result) {
  NetworkMetadataStore* network_metadata_store =
      NetworkHandler::IsInitialized()
          ? NetworkHandler::Get()->network_metadata_store()
          : nullptr;
  if (!network_metadata_store)
    return false;

  if (!is_connectable) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, it is not connectable.";
    }
    return false;
  }

  if (!network_metadata_store->GetIsCreatedByUser(guid)) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, was not configured by user.";
    }
    return false;
  }

  if (security_type != network_config::mojom::SecurityType::kWepPsk &&
      security_type != network_config::mojom::SecurityType::kWpaPsk) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, security type not supported: "
                     << security_type;
    }
    return false;
  }

  base::TimeDelta timestamp =
      network_metadata_store->GetLastConnectedTimestamp(guid);
  if (timestamp.is_zero()) {
    if (log_result) {
      NET_LOG(EVENT) << NetworkGuidId(guid)
                     << " is not eligible, never connected.";
    }
    return false;
  }

  if (log_result) {
    NET_LOG(EVENT) << NetworkGuidId(guid) << " is eligible for sync.";
  }
  return true;
}

}  // namespace sync_wifi

}  // namespace chromeos
