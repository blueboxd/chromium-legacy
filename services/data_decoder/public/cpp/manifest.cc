// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace data_decoder {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Data Decoder Service")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability(
              "image_decoder",
              service_manager::Manifest::InterfaceList<mojom::ImageDecoder>())
          .ExposeCapability(
              "json_parser",
              service_manager::Manifest::InterfaceList<mojom::JsonParser>())
          .ExposeCapability(
              "xml_parser",
              service_manager::Manifest::InterfaceList<mojom::XmlParser>())
          .ExposeCapability("bundled_exchanges_parser",
                            service_manager::Manifest::InterfaceList<
                                mojom::BundledExchangesParser>())
          .Build()};
  return *manifest;
}

}  // namespace data_decoder
