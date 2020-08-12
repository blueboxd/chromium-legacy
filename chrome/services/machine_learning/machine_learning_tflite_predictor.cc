// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/machine_learning_tflite_predictor.h"

#include "base/check.h"

namespace machine_learning {

TFLitePredictor::TFLitePredictor(std::string filename, int32_t num_threads)
    : model_file_name_(filename), num_threads_(num_threads) {}

TFLitePredictor::~TFLitePredictor() = default;

TfLiteStatus TFLitePredictor::Initialize() {
  if (!LoadModel())
    return kTfLiteError;
  if (!BuildInterpreter())
    return kTfLiteError;
  TfLiteStatus status = AllocateTensors();
  if (status == kTfLiteOk)
    initialized_ = true;
  return status;
}

TfLiteStatus TFLitePredictor::Evaluate() {
  return TfLiteInterpreterInvoke(interpreter_.get());
}

bool TFLitePredictor::LoadModel() {
  if (model_file_name_.empty())
    return false;

  // We create the pointer using this approach since |TfLiteModel| is a
  // structure without the delete operator.
  model_ = std::unique_ptr<TfLiteModel, std::function<void(TfLiteModel*)>>(
      TfLiteModelCreateFromFile(model_file_name_.c_str()), &TfLiteModelDelete);
  if (model_ == nullptr)
    return false;

  return true;
}

bool TFLitePredictor::BuildInterpreter() {
  // We create the pointer using this approach since |TfLiteInterpreterOptions|
  // is a structure without the delete operator.
  options_ = std::unique_ptr<TfLiteInterpreterOptions,
                             std::function<void(TfLiteInterpreterOptions*)>>(
      TfLiteInterpreterOptionsCreate(), &TfLiteInterpreterOptionsDelete);
  if (options_ == nullptr)
    return false;

  TfLiteInterpreterOptionsSetNumThreads(options_.get(), num_threads_);

  // We create the pointer using this approach since |TfLiteInterpreter| is a
  // structure without the delete operator.
  interpreter_ = std::unique_ptr<TfLiteInterpreter,
                                 std::function<void(TfLiteInterpreter*)>>(
      TfLiteInterpreterCreate(model_.get(), options_.get()),
      &TfLiteInterpreterDelete);
  if (interpreter_ == nullptr)
    return false;

  return true;
}

TfLiteStatus TFLitePredictor::AllocateTensors() {
  TfLiteStatus status = TfLiteInterpreterAllocateTensors(interpreter_.get());
  DCHECK(status == kTfLiteOk);
  return status;
}

int32_t TFLitePredictor::GetInputTensorCount() const {
  if (interpreter_ == nullptr)
    return 0;
  return TfLiteInterpreterGetInputTensorCount(interpreter_.get());
}

int32_t TFLitePredictor::GetOutputTensorCount() const {
  if (interpreter_ == nullptr)
    return 0;
  return TfLiteInterpreterGetOutputTensorCount(interpreter_.get());
}

TfLiteTensor* TFLitePredictor::GetInputTensor(int32_t index) const {
  if (interpreter_ == nullptr)
    return nullptr;
  return TfLiteInterpreterGetInputTensor(interpreter_.get(), index);
}

const TfLiteTensor* TFLitePredictor::GetOutputTensor(int32_t index) const {
  if (interpreter_ == nullptr)
    return nullptr;
  return TfLiteInterpreterGetOutputTensor(interpreter_.get(), index);
}

bool TFLitePredictor::IsInitialized() const {
  return initialized_;
}

int32_t TFLitePredictor::GetInputTensorNumDims(int32_t tensor_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  return TfLiteTensorNumDims(tensor);
}

int32_t TFLitePredictor::GetInputTensorDim(int32_t tensor_index,
                                           int32_t dim_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  return TfLiteTensorDim(tensor, dim_index);
}

void* TFLitePredictor::GetInputTensorData(int32_t tensor_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  return TfLiteTensorData(tensor);
}

int32_t TFLitePredictor::GetOutputTensorNumDims(int32_t tensor_index) const {
  const TfLiteTensor* tensor = GetOutputTensor(tensor_index);
  return TfLiteTensorNumDims(tensor);
}

int32_t TFLitePredictor::GetOutputTensorDim(int32_t tensor_index,
                                            int32_t dim_index) const {
  const TfLiteTensor* tensor = GetOutputTensor(tensor_index);
  return TfLiteTensorDim(tensor, dim_index);
}

void* TFLitePredictor::GetOutputTensorData(int32_t tensor_index) const {
  const TfLiteTensor* tensor = GetInputTensor(tensor_index);
  return TfLiteTensorData(tensor);
}

}  // namespace machine_learning
