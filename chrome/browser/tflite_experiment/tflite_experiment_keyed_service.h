// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_KEYED_SERVICE_H_
#define CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_KEYED_SERVICE_H_

#include "chrome/services/machine_learning/in_process_tflite_predictor.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

// Keyed service than can be used to receive requests for enabling
// a TFLite experiment.
class TFLiteExperimentKeyedService : public KeyedService {
 public:
  explicit TFLiteExperimentKeyedService(
      content::BrowserContext* browser_context);
  ~TFLiteExperimentKeyedService() override;

  machine_learning::InProcessTFLitePredictor* tflite_predictor() {
    return predictor_.get();
  }

 private:
  // The predictor owned by this keyed service capable of
  // running a TFLite model.
  std::unique_ptr<machine_learning::InProcessTFLitePredictor> predictor_;
};

#endif  // CHROME_BROWSER_TFLITE_EXPERIMENT_TFLITE_EXPERIMENT_KEYED_SERVICE_H_
