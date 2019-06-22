// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/cert/internal/crl.h"
#include "net/der/input.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::der::Input idp_der(data, size);

  std::unique_ptr<net::GeneralNames> distribution_point_names;

  if (net::ParseIssuingDistributionPoint(idp_der, &distribution_point_names)) {
    CHECK((distribution_point_names &&
           distribution_point_names->present_name_types !=
               net::GENERAL_NAME_NONE));
  }
  return 0;
}
