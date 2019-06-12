// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_parser/public/cpp/manifest.h"

#include <set>

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/cups_ipp_parser/public/mojom/constants.mojom.h"
#include "chrome/services/cups_ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetCupsIppParserManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(cups_ipp_parser::mojom::kCupsIppParserServiceName)
          .WithDisplayName(IDS_UTILITY_PROCESS_CUPS_IPP_PARSER_SERVICE_NAME)
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("utility")
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability("ipp_parser",
                            service_manager::Manifest::InterfaceList<
                                cups_ipp_parser::mojom::IppParser>())
          .Build()};
  return *manifest;
}
