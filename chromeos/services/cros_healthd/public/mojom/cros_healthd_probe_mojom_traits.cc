// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe_mojom_traits.h"

#include "base/notreached.h"

namespace em = enterprise_management;

namespace mojo {

chromeos::cros_healthd::mojom::CpuArchitectureEnum
EnumTraits<chromeos::cros_healthd::mojom::CpuArchitectureEnum,
           enterprise_management::CpuInfo::Architecture>::
    ToMojom(enterprise_management::CpuInfo::Architecture input) {
  switch (input) {
    case enterprise_management::CpuInfo::ARCHITECTURE_UNSPECIFIED:
      return chromeos::cros_healthd::mojom::CpuArchitectureEnum::kUnknown;
    case enterprise_management::CpuInfo::X86_64:
      return chromeos::cros_healthd::mojom::CpuArchitectureEnum::kX86_64;
  }

  NOTREACHED();
  return chromeos::cros_healthd::mojom::CpuArchitectureEnum::kUnknown;
}

bool EnumTraits<chromeos::cros_healthd::mojom::CpuArchitectureEnum,
                enterprise_management::CpuInfo::Architecture>::
    FromMojom(chromeos::cros_healthd::mojom::CpuArchitectureEnum input,
              enterprise_management::CpuInfo::Architecture* out) {
  switch (input) {
    case chromeos::cros_healthd::mojom::CpuArchitectureEnum::kUnknown:
      *out = enterprise_management::CpuInfo::ARCHITECTURE_UNSPECIFIED;
      return true;
    case chromeos::cros_healthd::mojom::CpuArchitectureEnum::kX86_64:
      *out = enterprise_management::CpuInfo::X86_64;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
