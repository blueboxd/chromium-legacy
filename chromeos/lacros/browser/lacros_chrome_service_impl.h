// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_
#define CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_

#include "base/component_export.h"
#include "chromeos/lacros/mojom/lacros.mojom.h"

namespace chromeos {

// Implements LacrosChromeService.
class COMPONENT_EXPORT(CHROMEOS_LACROS) LacrosChromeServiceImpl
    : public lacros::mojom::LacrosChromeService {
 public:
  LacrosChromeServiceImpl();
  ~LacrosChromeServiceImpl() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_BROWSER_LACROS_CHROME_SERVICE_IMPL_H_
