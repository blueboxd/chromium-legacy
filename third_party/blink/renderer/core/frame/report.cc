// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/report.h"

namespace blink {

constexpr const char ReportType::kDeprecation[];
constexpr const char ReportType::kFeaturePolicyViolation[];
constexpr const char ReportType::kIntervention[];
constexpr const char ReportType::kCSPViolation[];

}  // namespace blink
