// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ANDROID_CPU_AFFINITY_EXPERIMENTS_H_
#define CHROME_COMMON_ANDROID_CPU_AFFINITY_EXPERIMENTS_H_

namespace chrome {

// Setup CPU-affinity restriction experiments (e.g. to restrict execution to
// little cores only) for the current process, based on the feature list. Should
// be called during process startup after feature list initialization.
void InitializeCpuAffinityExperiments();

}  // namespace chrome

#endif  // CHROME_COMMON_ANDROID_CPU_AFFINITY_EXPERIMENTS_H_
