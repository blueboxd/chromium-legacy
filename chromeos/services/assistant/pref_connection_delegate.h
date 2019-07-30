// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PREF_CONNECTION_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PREF_CONNECTION_DELEGATE_H_

#include "base/macros.h"
#include "components/prefs/pref_registry_simple.h"
#include "services/preferences/public/cpp/pref_service_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

// Helper interface class to allow overriding pref services in tests.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) PrefConnectionDelegate {
 public:
  PrefConnectionDelegate() = default;
  virtual ~PrefConnectionDelegate() = default;

  virtual void ConnectToPrefService(
      service_manager::Connector* connector,
      scoped_refptr<PrefRegistrySimple> pref_registry,
      prefs::ConnectCallback callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefConnectionDelegate);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PREF_CONNECTION_DELEGATE_H_
