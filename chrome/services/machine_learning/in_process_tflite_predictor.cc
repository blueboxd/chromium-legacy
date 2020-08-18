// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/in_process_tflite_predictor.h"

#include "base/check.h"

namespace machine_learning {

InProcessTFLitePredictor::InProcessTFLitePredictor(std::string filename,
                                                   int32_t num_threads)
    : model_file_name_(filename), num_threads_(num_threads) {}

InProcessTFLitePredictor::~InProcessTFLitePredictor() = default;

TfLiteStatus InProcessTFLitePredictor::Initialize() {
  if (!LoadModel())
    return kTfLiteError;
  if (!BuildInterpreter())
    return kTfLiteError;
  TfLiteStatus status = AllocateTensors();
  if (status == kTfLiteOk)
    initialized_ = true;
  return status;
}

TfLiteStatus InProcessTFLitePredictor::Evaluate() {
  return TfLiteInterpreterInvoke(interpreter_.get());
}

bool InProcessTFLitePredictor::LoadModel() {
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

bool InProcessTFLitePredictor::BuildInterpreter() {
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

TfLiteStatus InProcessTFLitePredictor::AllocateTensors() {
  TfLiteStatus status = TfLiteInterpreterAllocateTensors(interpreter_.get());
  DCHECK(status == kTfLiteOk);
  return status;
}

int32_t InProcessTFLitePredictor::GetInputTensorCount() const {
  if (interpreter_ == nullptr)
    return 0;
  return TfLiteInterpreterGetInputTensorCount(interpreter_.get());
}

int32_t InProcessTFLitePredictor::GetOutputTensorCount() const {
  if (interpreter_ == nullptr)
    return 0;
  return TfLiteInterpreterGetOutputTensorCount(interpreter_.get());
}

TfLiteTensor* InProcessTFLitePredictor::GetInputTensor(int32_t index) const {
  if (interpreter_ == nullptr)
    return nullptr;
  return TfLiteInterpreterGetInputTensor(interpreter_.get(), index);
}

const TfLiteTensor* InProcessTFLitePredictor::GetOutputTensor(
    int32_t index) const {
  if (interpreter_ == nullptr)
    return nullptr;
  return TfLiteInterpreterGetOutputTensor(interpreter_.get(), index);
}

bool InProcessTFLitePredictor::IsInitialized() const {
  return initialized_;
}

int32_t InProcessTFLitePredictor::GetInputTensorNumDims(
    int32_t tensor_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  return TfLiteTensorNumDims(tensor);
}

int32_t InProcessTFLitePredictor::GetInputTensorDim(int32_t tensor_index,
                                                    int32_t dim_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  return TfLiteTensorDim(tensor, dim_index);
}

void* InProcessTFLitePredictor::GetInputTensorData(int32_t tensor_index) const {
  TfLiteTensor* tensor = GetInputTensor(tensor_index);
  return TfLiteTensorData(tensor);
}

int32_t InProcessTFLitePredictor::GetOutputTensorNumDims(
    int32_t tensor_index) const {
  const TfLiteTensor* tensor = GetOutputTensor(tensor_index);
  return TfLiteTensorNumDims(tensor);
}

int32_t InProcessTFLitePredictor::GetOutputTensorDim(int32_t tensor_index,
                                                     int32_t dim_index) const {
  const TfLiteTensor* tensor = GetOutputTensor(tensor_index);
  return TfLiteTensorDim(tensor, dim_index);
}

void* InProcessTFLitePredictor::GetOutputTensorData(
    int32_t tensor_index) const {
  const TfLiteTensor* tensor = GetInputTensor(tensor_index);
  return TfLiteTensorData(tensor);
}

int32_t InProcessTFLitePredictor::GetTFLiteNumThreads() const {
  return num_threads_;
}

}  // namespace machine_learning
