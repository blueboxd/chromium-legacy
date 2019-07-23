// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_renderer_manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/common/constants.mojom.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetChromeRendererManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(chrome::mojom::kRendererServiceName)
          .ExposeCapability("browser",
                            service_manager::Manifest::InterfaceList<
                                chrome::mojom::WebRtcLoggingAgent,
                                safe_browsing::mojom::PhishingModelSetter,
                                spellcheck::mojom::SpellChecker>())
          .RequireCapability(chrome::mojom::kServiceName, "renderer")
          .Build()};
  return *manifest;
}
