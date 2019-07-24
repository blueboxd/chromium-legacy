// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/builtin_service_manifests.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/services/heap_profiling/public/cpp/manifest.h"
#include "content/public/app/content_browser_manifest.h"
#include "content/public/app/content_gpu_manifest.h"
#include "content/public/app/content_plugin_manifest.h"
#include "content/public/app/content_renderer_manifest.h"
#include "content/public/app/content_utility_manifest.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_names.mojom.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/services/cdm_manifest.h"
#include "media/mojo/services/media_manifest.h"
#include "services/audio/public/cpp/manifest.h"
#include "services/data_decoder/public/cpp/manifest.h"
#include "services/device/public/cpp/manifest.h"
#include "services/media_session/public/cpp/manifest.h"
#include "services/metrics/public/cpp/manifest.h"
#include "services/network/public/cpp/manifest.h"
#include "services/resource_coordinator/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/shape_detection/public/cpp/manifest.h"
#include "services/tracing/manifest.h"

#if defined(OS_LINUX)
#include "components/services/font/public/cpp/manifest.h"  // nogncheck
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/assistant/buildflags.h"
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/assistant/public/cpp/audio_decoder_manifest.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)

namespace content {

namespace {

bool IsAudioServiceOutOfProcess() {
  return base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) &&
         !GetContentClient()->browser()->OverridesAudioManager();
}

}  // namespace

const std::vector<service_manager::Manifest>& GetBuiltinServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{
      std::vector<service_manager::Manifest>{
          GetContentBrowserManifest(),

          // NOTE: Content child processes are of course not running in the
          // browser process, but the distinction between "in-process" and
          // "out-of-process" manifests is temporary. For now, this is the right
          // place for these manifests.
          GetContentGpuManifest(),
          GetContentPluginManifest(),
          GetContentRendererManifest(),
          GetContentUtilityManifest(),

          heap_profiling::GetManifest(),
          audio::GetManifest(IsAudioServiceOutOfProcess()
                                 ? service_manager::Manifest::ExecutionMode::
                                       kOutOfProcessBuiltin
                                 : service_manager::Manifest::ExecutionMode::
                                       kInProcessBuiltin),

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
          media::GetCdmManifest(),
#endif
          media::GetMediaManifest(),
          data_decoder::GetManifest(),
          device::GetManifest(),
          media_session::GetManifest(),
          metrics::GetManifest(),
          network::GetManifest(
              IsInProcessNetworkService()
                  ? service_manager::Manifest::ExecutionMode::kInProcessBuiltin
                  : service_manager::Manifest::ExecutionMode::
                        kOutOfProcessBuiltin),
          resource_coordinator::GetManifest(),
          shape_detection::GetManifest(),
          tracing::GetManifest(),
#if defined(OS_LINUX)
          font_service::GetManifest(),
#endif
#if defined(OS_CHROMEOS)
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
          // TODO(https://crbug.com/929340): This doesn't belong here!
          chromeos::assistant::GetAudioDecoderManifest(),
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)
      }};
  return *manifests;
}

}  // namespace content
