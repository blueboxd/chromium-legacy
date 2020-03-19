// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_AGENT_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_AGENT_UTIL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"

namespace chromeos {
namespace power {
namespace ml {

using ::chromeos::machine_learning::mojom::CreateGraphExecutorResult;
using ::chromeos::machine_learning::mojom::LoadModelResult;

// Extract essential fields from parsed JSON dict.
bool ParseMetaInfoFromJsonObject(const base::Value& root,
                                 std::string* metrics_model_name,
                                 double* dim_threshold,
                                 size_t* expected_feature_size,
                                 base::flat_map<std::string, int>* inputs,
                                 base::flat_map<std::string, int>* outputs);

// Callbacks for LoadModel and CreateGraphExecutor used by BuiltinWorker and
// DownloadWorker.
void LoadModelCallback(LoadModelResult result);

void CreateGraphExecutorCallback(CreateGraphExecutorResult result);

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_ML_AGENT_UTIL_H_
