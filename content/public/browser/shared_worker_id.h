// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SHARED_WORKER_ID_H_
#define CONTENT_PUBLIC_BROWSER_SHARED_WORKER_ID_H_

#include "base/util/type_safety/id_type.h"

namespace content {

using SharedWorkerId = util::IdType64<class SharedWorkerTag>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SHARED_WORKER_ID_H_
