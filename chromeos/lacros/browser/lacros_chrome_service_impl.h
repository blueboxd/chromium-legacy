// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_
#define CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_

#include "base/component_export.h"
#include "chromeos/lacros/mojom/lacros.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// Implements LacrosChromeService.
class COMPONENT_EXPORT(CHROMEOS_LACROS) LacrosChromeServiceImpl
    : public lacros::mojom::LacrosChromeService {
 public:
  // TODO(hidehiko): Add static getter of the instance.
  // The instance of this class should be globally unique.

  LacrosChromeServiceImpl();
  ~LacrosChromeServiceImpl() override;

  // lacros::mojom::LacrosChromeService:
  void RequestAshChromeServiceReceiver(
      RequestAshChromeServiceReceiverCallback callback) override;

 private:
  // Proxy to AshChromeService in ash-chrome.
  // TODO(hidehiko): Add getter for Remote<AshChromeService>.
  mojo::Remote<lacros::mojom::AshChromeService> ash_chrome_service_;

  // Pending receiver of AshChromeService.
  // AshChromeService is bound to mojo::Remote on construction, then
  // when AshChromeService requests via RequestAshChromeServiceReceiver,
  // its PendingReceiver is returned.
  // This member holds the PendingReceiver between them. Note that even
  // during the period, calling a method on AshChromeService via Remote
  // should be available.
  mojo::PendingReceiver<lacros::mojom::AshChromeService>
      pending_ash_chrome_service_receiver_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_
