// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/image_annotation_service.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace image_annotation {

// static
const base::Feature ImageAnnotationService::kExperiment{
    "ImageAnnotationServiceExperimental", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<std::string> ImageAnnotationService::kServerUrl;
constexpr base::FeatureParam<std::string> ImageAnnotationService::kApiKey;
constexpr base::FeatureParam<int> ImageAnnotationService::kThrottleMs;
constexpr base::FeatureParam<int> ImageAnnotationService::kBatchSize;
constexpr base::FeatureParam<double> ImageAnnotationService::kMinOcrConfidence;

ImageAnnotationService::ImageAnnotationService(
    service_manager::mojom::ServiceRequest request,
    std::string api_key,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : service_binding_(this, std::move(request)),
      annotator_(GURL(kServerUrl.Get()),
                 kApiKey.Get().empty() ? std::move(api_key) : kApiKey.Get(),
                 base::TimeDelta::FromMilliseconds(kThrottleMs.Get()),
                 kBatchSize.Get(),
                 kMinOcrConfidence.Get(),
                 shared_url_loader_factory,
                 service_binding_.GetConnector()) {}

ImageAnnotationService::~ImageAnnotationService() = default;

void ImageAnnotationService::OnStart() {
  registry_.AddInterface<mojom::Annotator>(base::BindRepeating(
      &Annotator::BindReceiver, base::Unretained(&annotator_)));
}

// service_manager::Service:
void ImageAnnotationService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace image_annotation
