// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LACROS_ASH_CHROME_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LACROS_ASH_CHROME_SERVICE_IMPL_H_

#include "chromeos/lacros/mojom/lacros.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

// Implementation of AshChromeService. It provides a set of APIs that
// lacros-chrome can call into.
class AshChromeServiceImpl : public lacros::mojom::AshChromeService {
 public:
  explicit AshChromeServiceImpl(
      mojo::PendingReceiver<lacros::mojom::AshChromeService> pending_receiver);
  ~AshChromeServiceImpl() override;

 private:
  mojo::Receiver<lacros::mojom::AshChromeService> receiver_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LACROS_ASH_CHROME_SERVICE_IMPL_H_
