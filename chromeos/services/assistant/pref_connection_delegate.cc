// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/pref_connection_delegate.h"
#include "base/token.h"
#include "services/preferences/public/cpp/pref_service_factory.h"

#include <utility>

namespace chromeos {
namespace assistant {

void PrefConnectionDelegate::ConnectToPrefService(
    mojo::PendingRemote<prefs::mojom::PrefStoreConnector> connector,
    scoped_refptr<PrefRegistrySimple> pref_registry,
    prefs::ConnectCallback callback) {
  prefs::mojom::PrefStoreConnectorPtr ptr(std::move(connector));
  ::prefs::ConnectToPrefService(std::move(ptr), std::move(pref_registry),
                                base::Token::CreateRandom(), callback);
}

}  // namespace assistant
}  // namespace chromeos
