// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tflite_experiment/tflite_experiment_observer.h"

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tflite_experiment/tflite_experiment_keyed_service.h"
#include "chrome/browser/tflite_experiment/tflite_experiment_keyed_service_factory.h"
#include "chrome/browser/tflite_experiment/tflite_experiment_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

constexpr int32_t kTFLitePredictorEvaluationLoop = 10;

namespace {

// Returns the TFLitePredictor.
machine_learning::TFLitePredictor* GetTFLitePredictorFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  if (Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
    return TFLiteExperimentKeyedServiceFactory::GetForProfile(profile)
        ->tflite_predictor();
  }
  return nullptr;
}

}  // namespace

TFLiteExperimentObserver::TFLiteExperimentObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  tflite_predictor_ = GetTFLitePredictorFromWebContents(web_contents);
  tflite_experiment_log_path_ =
      tflite_experiment::switches::GetTFLiteExperimentLogPath();
}

TFLiteExperimentObserver::~TFLiteExperimentObserver() = default;

void TFLiteExperimentObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(navigation_handle);
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->GetURL().SchemeIsHTTPOrHTTPS() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (!tflite_predictor_ || !tflite_predictor_->IsInitialized()) {
    LOCAL_HISTOGRAM_BOOLEAN("TFLiteExperiment.Observer.TFLitePredictor.Null",
                            true);
    return;
  }

  if (is_tflite_evaluated_)
    return;

  base::TimeTicks t1 = base::TimeTicks::Now();
  CreatePredictorInputForTesting();
  base::TimeTicks t2 = base::TimeTicks::Now();
  // Run evaluation for |kTFLitePredictorEvaluationLoop| times and report
  // average time.
  for (int i = 0; i < kTFLitePredictorEvaluationLoop; i++)
    tflite_predictor_->Evaluate();
  base::TimeTicks t3 = base::TimeTicks::Now();

  log_dict_.SetIntKey("input_set_time", (t2 - t1).InMicroseconds());
  log_dict_.SetIntKey("evaluation_time", (t3 - t2).InMilliseconds() /
                                             kTFLitePredictorEvaluationLoop);

  is_tflite_evaluated_ = true;
  LOCAL_HISTOGRAM_BOOLEAN(
      "TFLiteExperiment.Observer.TFLitePredictor.EvaluationRequested", true);

  std::string message;
  base::JSONWriter::Write(log_dict_, &message);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LogDictionary, tflite_experiment_log_path_, message));
}

void TFLiteExperimentObserver::CreatePredictorInputForTesting() {
  for (int i = 0; i < tflite_predictor_->GetInputTensorCount(); i++) {
    int32_t tensor_total_dim = 1;
    for (int j = 0; j < tflite_predictor_->GetInputTensorNumDims(i); j++) {
      tensor_total_dim =
          tensor_total_dim * tflite_predictor_->GetInputTensorDim(i, j);
    }
    int* tensor_data =
        static_cast<int*>(tflite_predictor_->GetInputTensorData(i));
    for (int k = 0; k < tensor_total_dim; k++) {
      tensor_data[k] = 1;
    }
  }
}

// static
void TFLiteExperimentObserver::Log(base::Optional<std::string> log_path,
                                   const std::string& data) {
  if (!log_path)
    return;
  base::FilePath log_file = base::FilePath(log_path.value());
  base::AppendToFile(log_file, data.c_str(), data.size());
}

// static
void TFLiteExperimentObserver::LogWriteHeader(
    base::Optional<std::string> log_path) {
  if (!log_path)
    return;
  base::FilePath log_file = base::FilePath(log_path.value());
  base::WriteFile(log_file, "", 0);
}

// static
void TFLiteExperimentObserver::LogDictionary(
    base::Optional<std::string> log_path,
    const std::string& data) {
  LogWriteHeader(log_path);
  Log(log_path, data);
  LOCAL_HISTOGRAM_BOOLEAN("TFLiteExperiment.Observer.Finish", true);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TFLiteExperimentObserver)
