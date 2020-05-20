// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/common_mac.h"

#import <ApplicationServices/ApplicationServices.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"

namespace quarantine {

bool GetQuarantineProperties(
    const base::FilePath& file,
    base::scoped_nsobject<NSMutableDictionary>* properties) {
  return false;
}

}  // namespace quarantine
