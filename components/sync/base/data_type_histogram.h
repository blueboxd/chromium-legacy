// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_
#define COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"

// Converts memory size |bytes| into kilobytes and records it into |model_type|
// related histogram for memory footprint of sync data.
void SyncRecordModelTypeMemoryHistogram(syncer::ModelType model_type,
                                        size_t bytes);

// Records |count| into a |model_type| related histogram for count of sync
// entities.
void SyncRecordModelTypeCountHistogram(syncer::ModelType model_type,
                                       size_t count);

#endif  // COMPONENTS_SYNC_BASE_DATA_TYPE_HISTOGRAM_H_
