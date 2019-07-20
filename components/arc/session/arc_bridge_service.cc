// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_bridge_service.h"

// These header is necessary for instantiation of ConnectionHolder.
#include "components/arc/common/accessibility_helper.mojom.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/common/app_permissions.mojom.h"
#include "components/arc/common/appfuse.mojom.h"
#include "components/arc/common/arc_bridge.mojom.h"
#include "components/arc/common/audio.mojom.h"
#include "components/arc/common/auth.mojom.h"
#include "components/arc/common/backup_settings.mojom.h"
#include "components/arc/common/bluetooth.mojom.h"
#include "components/arc/common/boot_phase_monitor.mojom.h"
#include "components/arc/common/camera.mojom.h"
#include "components/arc/common/cast_receiver.mojom.h"
#include "components/arc/common/cert_store.mojom.h"
#include "components/arc/common/clipboard.mojom.h"
#include "components/arc/common/crash_collector.mojom.h"
#include "components/arc/common/disk_quota.mojom.h"
#include "components/arc/common/enterprise_reporting.mojom.h"
#include "components/arc/common/file_system.mojom.h"
#include "components/arc/common/ime.mojom.h"
#include "components/arc/common/input_method_manager.mojom.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/common/keymaster.mojom.h"
#include "components/arc/common/kiosk.mojom.h"
#include "components/arc/common/lock_screen.mojom.h"
#include "components/arc/common/media_session.mojom.h"
#include "components/arc/common/metrics.mojom.h"
#include "components/arc/common/midis.mojom.h"
#include "components/arc/common/net.mojom.h"
#include "components/arc/common/obb_mounter.mojom.h"
#include "components/arc/common/oemcrypto.mojom.h"
#include "components/arc/common/pip.mojom.h"
#include "components/arc/common/policy.mojom.h"
#include "components/arc/common/power.mojom.h"
#include "components/arc/common/print.mojom.h"
#include "components/arc/common/print_spooler.mojom.h"
#include "components/arc/common/process.mojom.h"
#include "components/arc/common/property.mojom.h"
#include "components/arc/common/rotation_lock.mojom.h"
#include "components/arc/common/screen_capture.mojom.h"
#include "components/arc/common/storage_manager.mojom.h"
#include "components/arc/common/timer.mojom.h"
#include "components/arc/common/tracing.mojom.h"
#include "components/arc/common/tts.mojom.h"
#include "components/arc/common/usb_host.mojom.h"
#include "components/arc/common/video.mojom.h"
#include "components/arc/common/voice_interaction_arc_home.mojom.h"
#include "components/arc/common/voice_interaction_framework.mojom.h"
#include "components/arc/common/volume_mounter.mojom.h"
#include "components/arc/common/wake_lock.mojom.h"
#include "components/arc/common/wallpaper.mojom.h"

namespace arc {

ArcBridgeService::ArcBridgeService() = default;

ArcBridgeService::~ArcBridgeService() = default;

}  // namespace arc
