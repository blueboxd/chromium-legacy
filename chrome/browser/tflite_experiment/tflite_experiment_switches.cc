// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tflite_experiment/tflite_experiment_switches.h"

#include "base/command_line.h"

namespace tflite_experiment {
namespace switches {

// Specifies the TFLite model path that TFLite machine learning uses.
const char kTFLiteModelPath[] = "tflite-model-path";

// Specifies the TFLite experiment log file path.
const char kTFLiteExperimentLogPath[] = "tflite-experiment-log-path";

base::Optional<std::string> GetTFLiteModelPath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(tflite_experiment::switches::kTFLiteModelPath)) {
    return command_line->GetSwitchValueASCII(
        tflite_experiment::switches::kTFLiteModelPath);
  }
  return base::nullopt;
}

base::Optional<std::string> GetTFLiteExperimentLogPath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          tflite_experiment::switches::kTFLiteExperimentLogPath)) {
    return command_line->GetSwitchValueASCII(
        tflite_experiment::switches::kTFLiteExperimentLogPath);
  }
  return base::nullopt;
}

}  // namespace switches
}  // namespace tflite_experiment
