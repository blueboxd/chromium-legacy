// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/pref_connection_delegate.h"

#include <utility>

namespace chromeos {
namespace assistant {

void PrefConnectionDelegate::ConnectToPrefService(
    service_manager::Connector* connector,
    scoped_refptr<PrefRegistrySimple> pref_registry,
    prefs::ConnectCallback callback) {
  ::prefs::ConnectToPrefService(connector, std::move(pref_registry), callback,
                                ::prefs::mojom::kServiceName);
}

}  // namespace assistant
}  // namespace chromeos
