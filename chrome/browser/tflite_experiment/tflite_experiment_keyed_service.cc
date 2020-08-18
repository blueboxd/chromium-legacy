// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tflite_experiment/tflite_experiment_keyed_service.h"

#include "base/optional.h"
#include "chrome/browser/tflite_experiment/tflite_experiment_switches.h"

TFLiteExperimentKeyedService::TFLiteExperimentKeyedService(
    content::BrowserContext* browser_context) {
  base::Optional<std::string> model_path =
      tflite_experiment::switches::GetTFLiteModelPath();
  if (!model_path)
    return;

  predictor_ = std::make_unique<machine_learning::InProcessTFLitePredictor>(
      model_path.value(),
      tflite_experiment::switches::GetTFLitePredictorNumThreads());
  predictor_->Initialize();
}

TFLiteExperimentKeyedService::~TFLiteExperimentKeyedService() = default;
