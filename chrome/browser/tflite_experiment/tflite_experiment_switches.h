// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_SWITCHES_H_
#define CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_SWITCHES_H_

#include <string>

#include "base/optional.h"

namespace tflite_experiment {
namespace switches {

extern const char kTFLiteModelPath[];
extern const char kTFLiteExperimentLogPath[];
extern const char kTFLitePredictorNumThreads[];

// Returns TFLite model path.
base::Optional<std::string> GetTFLiteModelPath();

// Returns TFLite experiment log file path.
base::Optional<std::string> GetTFLiteExperimentLogPath();

// Returns TFLite predictor number of threads.
int32_t GetTFLitePredictorNumThreads();

}  // namespace switches
}  // namespace tflite_experiment

#endif  // CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_SWITCHES_H_
