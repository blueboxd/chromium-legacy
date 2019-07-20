// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_arc_bridge_host.h"

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
#include "components/arc/common/notifications.mojom.h"
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

FakeArcBridgeHost::FakeArcBridgeHost() = default;

FakeArcBridgeHost::~FakeArcBridgeHost() = default;

void FakeArcBridgeHost::OnAccessibilityHelperInstanceReady(
    mojom::AccessibilityHelperInstancePtr accessibility_helper_ptr) {}

void FakeArcBridgeHost::OnAppInstanceReady(mojom::AppInstancePtr app_ptr) {}

void FakeArcBridgeHost::OnAppPermissionsInstanceReady(
    mojom::AppPermissionsInstancePtr app_permissions_ptr) {}

void FakeArcBridgeHost::OnAppfuseInstanceReady(
    mojom::AppfuseInstancePtr app_ptr) {}

void FakeArcBridgeHost::OnAudioInstanceReady(
    mojom::AudioInstancePtr audio_ptr) {}

void FakeArcBridgeHost::OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) {}

void FakeArcBridgeHost::OnBackupSettingsInstanceReady(
    mojom::BackupSettingsInstancePtr backup_settings_ptr) {}

void FakeArcBridgeHost::OnBluetoothInstanceReady(
    mojom::BluetoothInstancePtr bluetooth_ptr) {}

void FakeArcBridgeHost::OnBootPhaseMonitorInstanceReady(
    mojom::BootPhaseMonitorInstancePtr boot_phase_monitor_ptr) {}

void FakeArcBridgeHost::OnCameraInstanceReady(
    mojom::CameraInstancePtr camera_ptr) {}

void FakeArcBridgeHost::OnCastReceiverInstanceReady(
    mojom::CastReceiverInstancePtr cast_receiver_ptr) {}

void FakeArcBridgeHost::OnCertStoreInstanceReady(
    mojom::CertStoreInstancePtr instance_ptr) {}

void FakeArcBridgeHost::OnClipboardInstanceReady(
    mojom::ClipboardInstancePtr clipboard_ptr) {}

void FakeArcBridgeHost::OnCrashCollectorInstanceReady(
    mojom::CrashCollectorInstancePtr crash_collector_ptr) {}

void FakeArcBridgeHost::OnDiskQuotaInstanceReady(
    mojom::DiskQuotaInstancePtr disk_quota_ptr) {}

void FakeArcBridgeHost::OnEnterpriseReportingInstanceReady(
    mojom::EnterpriseReportingInstancePtr enterprise_reporting_ptr) {}

void FakeArcBridgeHost::OnFileSystemInstanceReady(
    mojom::FileSystemInstancePtr file_system_ptr) {}

void FakeArcBridgeHost::OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) {}

void FakeArcBridgeHost::OnInputMethodManagerInstanceReady(
    mojom::InputMethodManagerInstancePtr input_method_manager_ptr) {}

void FakeArcBridgeHost::OnIntentHelperInstanceReady(
    mojom::IntentHelperInstancePtr intent_helper_ptr) {}

void FakeArcBridgeHost::OnKeymasterInstanceReady(
    mojom::KeymasterInstancePtr keymaster_ptr) {}

void FakeArcBridgeHost::OnKioskInstanceReady(
    mojom::KioskInstancePtr kiosk_ptr) {}

void FakeArcBridgeHost::OnLockScreenInstanceReady(
    mojom::LockScreenInstancePtr lock_screen_ptr) {}

void FakeArcBridgeHost::OnMediaSessionInstanceReady(
    mojom::MediaSessionInstancePtr media_sesssion_ptr) {}

void FakeArcBridgeHost::OnMetricsInstanceReady(
    mojom::MetricsInstancePtr metrics_ptr) {}

void FakeArcBridgeHost::OnMidisInstanceReady(
    mojom::MidisInstancePtr midis_ptr) {}

void FakeArcBridgeHost::OnNetInstanceReady(mojom::NetInstancePtr net_ptr) {}

void FakeArcBridgeHost::OnNotificationsInstanceReady(
    mojom::NotificationsInstancePtr notifications_ptr) {}

void FakeArcBridgeHost::OnObbMounterInstanceReady(
    mojom::ObbMounterInstancePtr obb_mounter_ptr) {}

void FakeArcBridgeHost::OnOemCryptoInstanceReady(
    mojom::OemCryptoInstancePtr oemcrypto_ptr) {}

void FakeArcBridgeHost::OnPipInstanceReady(mojom::PipInstancePtr pip_ptr) {}

void FakeArcBridgeHost::OnPolicyInstanceReady(
    mojom::PolicyInstancePtr policy_ptr) {}

void FakeArcBridgeHost::OnPowerInstanceReady(
    mojom::PowerInstancePtr power_ptr) {}

void FakeArcBridgeHost::OnPrintInstanceReady(
    mojom::PrintInstancePtr print_ptr) {}

void FakeArcBridgeHost::OnPrintSpoolerInstanceReady(
    mojom::PrintSpoolerInstancePtr print_spooler_ptr) {}

void FakeArcBridgeHost::OnProcessInstanceReady(
    mojom::ProcessInstancePtr process_ptr) {}

void FakeArcBridgeHost::OnPropertyInstanceReady(
    mojom::PropertyInstancePtr property_ptr) {}

void FakeArcBridgeHost::OnRotationLockInstanceReady(
    mojom::RotationLockInstancePtr rotation_lock_ptr) {}

void FakeArcBridgeHost::OnScreenCaptureInstanceReady(
    mojom::ScreenCaptureInstancePtr screen_capture_ptr) {}

void FakeArcBridgeHost::OnStorageManagerInstanceReady(
    mojom::StorageManagerInstancePtr storage_manager_ptr) {}

void FakeArcBridgeHost::OnTimerInstanceReady(
    mojom::TimerInstancePtr timer_ptr) {}

void FakeArcBridgeHost::OnTracingInstanceReady(
    mojom::TracingInstancePtr trace_ptr) {}

void FakeArcBridgeHost::OnTtsInstanceReady(mojom::TtsInstancePtr tts_ptr) {}

void FakeArcBridgeHost::OnUsbHostInstanceReady(
    mojom::UsbHostInstancePtr usb_ptr) {}

void FakeArcBridgeHost::OnVideoInstanceReady(
    mojom::VideoInstancePtr video_ptr) {}

void FakeArcBridgeHost::OnVoiceInteractionArcHomeInstanceReady(
    mojom::VoiceInteractionArcHomeInstancePtr home_ptr) {}

void FakeArcBridgeHost::OnVoiceInteractionFrameworkInstanceReady(
    mojom::VoiceInteractionFrameworkInstancePtr framework_ptr) {}

void FakeArcBridgeHost::OnVolumeMounterInstanceReady(
    mojom::VolumeMounterInstancePtr volume_mounter_ptr) {}

void FakeArcBridgeHost::OnWakeLockInstanceReady(
    mojom::WakeLockInstancePtr wakelock_ptr) {}

void FakeArcBridgeHost::OnWallpaperInstanceReady(
    mojom::WallpaperInstancePtr wallpaper_ptr) {}

}  // namespace arc
