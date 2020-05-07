// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_data_describer_util.h"

#include "base/i18n/time_formatting.h"

namespace performance_manager {

base::Value TimeDeltaFromNowToValue(base::TimeTicks time_ticks) {
  base::TimeDelta delta = base::TimeTicks::Now() - time_ticks;

  base::string16 out;
  bool succeeded = TimeDurationFormat(delta, base::DURATION_WIDTH_WIDE, &out);
  DCHECK(succeeded);

  return base::Value(out);
}

}  // namespace performance_manager
