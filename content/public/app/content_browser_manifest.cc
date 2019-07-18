// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_browser_manifest.h"

#include "base/no_destructor.h"
#include "content/public/common/service_names.mojom.h"
#include "services/content/public/cpp/manifest.h"
#include "services/file/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace content {

const service_manager::Manifest& GetContentBrowserManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kBrowserServiceName)
          .WithDisplayName("Content (browser process)")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .CanConnectToInstancesInAnyGroup(true)
                           .CanConnectToInstancesWithAnyId(true)
                           .CanRegisterOtherServiceInstances(true)
                           .Build())
          .ExposeCapability("field_trials",
                            std::set<const char*>{
                                "content.mojom.FieldTrialRecorder",
                            })
          .ExposeCapability("sandbox_support",
                            std::set<const char*>{
                                "content.mojom.SandboxSupportMac",
                            })
          .ExposeCapability("font_cache",
                            std::set<const char*>{
                                "content.mojom.FontCacheWin",
                            })
          .ExposeCapability(
              "plugin",
              std::set<const char*>{
                  "discardable_memory.mojom.DiscardableSharedMemoryManager",
                  "viz.mojom.Gpu",
              })
          .ExposeCapability(
              "app",
              std::set<const char*>{
                  "discardable_memory.mojom.DiscardableSharedMemoryManager",
                  "memory_instrumentation.mojom.Coordinator",
              })
          .ExposeCapability("dwrite_font_proxy",
                            std::set<const char*>{
                                "blink.mojom.DWriteFontProxy",
                            })
          .ExposeCapability(
              "renderer",
              std::set<const char*>{
                  "blink.mojom.AecDumpManager",
                  "blink.mojom.AppCacheBackend",
                  "blink.mojom.BlobRegistry",
                  "blink.mojom.BroadcastChannelProvider",
                  "blink.mojom.ClipboardHost",
                  "blink.mojom.CodeCacheHost",
                  "blink.mojom.FontUniqueNameLookup",
                  "blink.mojom.EmbeddedFrameSinkProvider",
                  "blink.mojom.FileUtilitiesHost",
                  "blink.mojom.FileSystemManager",
                  "blink.mojom.Hyphenation",
                  "blink.mojom.MediaStreamTrackMetricsHost",
                  "blink.mojom.MimeRegistry",
                  "blink.mojom.OneShotBackgroundSyncService",
                  "blink.mojom.PeriodicBackgroundSyncService",
                  "blink.mojom.PluginRegistry",
                  "blink.mojom.PushMessaging",
                  "blink.mojom.ReportingServiceProxy",
                  "blink.mojom.StoragePartitionService",
                  "blink.mojom.WebDatabaseHost",
                  "content.mojom.ClipboardHost",
                  "content.mojom.FieldTrialRecorder",
                  "content.mojom.FrameSinkProvider",
                  "content.mojom.PeerConnectionTrackerHost",
                  "content.mojom.RendererHost",
                  "content.mojom.ReportingServiceProxy",
                  "content.mojom.WorkerURLLoaderFactoryProvider",
                  "device.mojom.BatteryMonitor",
                  "device.mojom.GamepadHapticsManager",
                  "discardable_memory.mojom.DiscardableSharedMemoryManager",
                  "media.mojom.KeySystemSupport",
                  "media.mojom.InterfaceFactory",
                  "media.mojom.VideoCaptureHost",
                  "metrics.mojom.SingleSampleMetricsProvider",
                  "midi.mojom.MidiSessionProvider",
                  "network.mojom.P2PSocketManager",
                  "network.mojom.MdnsResponder",
                  "network.mojom.URLLoaderFactory",
                  "resource_coordinator.mojom.ProcessCoordinationUnit",
                  "viz.mojom.CompositingModeReporter",
                  "viz.mojom.Gpu",
              })
          .ExposeCapability("gpu_client",
                            std::set<const char*>{
                                "viz.mojom.Gpu",
                            })
          .ExposeCapability(
              "gpu",
              std::set<const char*>{
                  "discardable_memory.mojom.DiscardableSharedMemoryManager",
                  "media.mojom.AndroidOverlayProvider",
              })
          .RequireCapability("data_decoder", "image_decoder")
          .RequireCapability("data_decoder", "json_parser")
          .RequireCapability("data_decoder", "xml_parser")
          .RequireCapability("cdm", "media:cdm")
          .RequireCapability("shape_detection", "barcode_detection")
          .RequireCapability("shape_detection", "face_detection")
          .RequireCapability("shape_detection", "text_detection")
          .RequireCapability("file", "file:filesystem")
          .RequireCapability("file", "file:leveldb")
          .RequireCapability("network", "network_service")
          .RequireCapability("network", "test")
          .RequireCapability(mojom::kRendererServiceName, "browser")
          .RequireCapability("media", "media:media")
          .RequireCapability("*", "app")
          .RequireCapability("content", "navigation")
          .RequireCapability("resource_coordinator", "service_callbacks")
          .RequireCapability("service_manager",
                             "service_manager:service_manager")
          .RequireCapability("chromecast", "multizone")
          .RequireCapability("content_plugin", "browser")
          .RequireCapability("metrics", "url_keyed_metrics")
          .RequireCapability("content_utility", "browser")
          .RequireCapability("device", "device:battery_monitor")
          .RequireCapability("device", "device:bluetooth_system")
          .RequireCapability("device", "device:generic_sensor")
          .RequireCapability("device", "device:geolocation")
          .RequireCapability("device", "device:hid")
          .RequireCapability("device", "device:input_service")
          .RequireCapability("device", "device:mtp")
          .RequireCapability("device", "device:nfc")
          .RequireCapability("device", "device:serial")
          .RequireCapability("device", "device:usb")
          .RequireCapability("device", "device:usb_test")
          .RequireCapability("device", "device:vibration")
          .RequireCapability("device", "device:wake_lock")
          .RequireCapability("media_session", "media_session:app")
          .RequireCapability("video_capture", "capture")
          .RequireCapability("video_capture", "tests")
          .RequireCapability("unzip_service", "unzip_file")
          .RequireCapability("tracing", "tracing")
          .RequireCapability("patch_service", "patch_file")
          .RequireCapability("audio", "info")
          .RequireCapability("audio", "debug_recording")
          .RequireCapability("audio", "device_notifier")
          .RequireCapability("audio", "log_factory_manager")
          .RequireCapability("audio", "stream_factory")
          .RequireCapability("audio", "testing_api")
          .RequireCapability("content_gpu", "browser")
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:shared_worker", "renderer",
              std::set<const char*>{
                  "blink.mojom.CacheStorage", "blink.mojom.FileSystemManager",
                  "blink.mojom.IDBFactory", "blink.mojom.LockManager",
                  "blink.mojom.NativeFileSystemManager",
                  "blink.mojom.NotificationService",
                  "blink.mojom.PermissionService",
                  "blink.mojom.QuotaDispatcherHost",
                  "blink.mojom.WebSocketConnector",
                  "media.mojom.VideoDecodePerfHistory",
                  "payments.mojom.PaymentManager",
                  "shape_detection.mojom.BarcodeDetectionProvider",
                  "shape_detection.mojom.FaceDetectionProvider",
                  "shape_detection.mojom.TextDetection"})
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:dedicated_worker", "renderer",
              std::set<const char*>{
                  "blink.mojom.CacheStorage",
                  "blink.mojom.DedicatedWorkerHostFactory",
                  "blink.mojom.FileSystemManager", "blink.mojom.IDBFactory",
                  "blink.mojom.IdleManager", "blink.mojom.LockManager",
                  "blink.mojom.NativeFileSystemManager",
                  "blink.mojom.NotificationService",
                  "blink.mojom.PermissionService",
                  "blink.mojom.QuotaDispatcherHost",
                  "blink.mojom.SerialService", "blink.mojom.WebUsbService",
                  "blink.mojom.SmsReceiver", "blink.mojom.WebSocketConnector",
                  "media.mojom.VideoDecodePerfHistory",
                  "payments.mojom.PaymentManager",
                  "shape_detection.mojom.BarcodeDetectionProvider",
                  "shape_detection.mojom.FaceDetectionProvider",
                  "shape_detection.mojom.TextDetection"})
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:service_worker", "renderer",
              std::set<const char*>{
                  "blink.mojom.BackgroundFetchService",
                  "blink.mojom.CacheStorage", "blink.mojom.CookieStore",
                  "blink.mojom.ContentIndexService", "blink.mojom.IDBFactory",
                  "blink.mojom.LockManager",
                  "blink.mojom.NativeFileSystemManager",
                  "blink.mojom.NotificationService",
                  "blink.mojom.PermissionService",
                  "blink.mojom.QuotaDispatcherHost",
                  "media.mojom.VideoDecodePerfHistory",
                  "network.mojom.RestrictedCookieManager",
                  "blink.mojom.WebSocketConnector",
                  "payments.mojom.PaymentManager",
                  "shape_detection.mojom.BarcodeDetectionProvider",
                  "shape_detection.mojom.FaceDetectionProvider",
                  "shape_detection.mojom.TextDetection"})
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "renderer",
              std::set<const char*>{
                  "autofill.mojom.AutofillDriver",
                  "autofill.mojom.PasswordManagerDriver",
                  "blink.mojom.AnchorElementMetricsHost",
                  "blink.mojom.BackgroundFetchService",
                  "blink.mojom.CacheStorage",
                  "blink.mojom.ColorChooserFactory",
                  "blink.mojom.ContactsManager",
                  "blink.mojom.ContentIndexService",
                  "blink.mojom.DateTimeChooser",
                  "blink.mojom.DisplayCutoutHost",
                  "blink.mojom.DedicatedWorkerHostFactory",
                  "blink.mojom.FileChooser",
                  "blink.mojom.FileSystemManager",
                  "blink.mojom.GeolocationService",
                  "blink.mojom.HidService",
                  "blink.mojom.IDBFactory",
                  "blink.mojom.IdleManager",
                  "blink.mojom.InsecureInputService",
                  "blink.mojom.KeyboardLockService",
                  "blink.mojom.LockManager",
                  "blink.mojom.MediaDevicesDispatcherHost",
                  "blink.mojom.MediaStreamDispatcherHost",
                  "blink.mojom.MediaSessionService",
                  "blink.mojom.NativeFileSystemManager",
                  "blink.mojom.NotificationService",
                  "blink.mojom.PermissionService",
                  "blink.mojom.PictureInPictureService",
                  "blink.mojom.Portal",
                  "blink.mojom.PrefetchURLLoaderService",
                  "blink.mojom.PresentationService",
                  "blink.mojom.QuotaDispatcherHost",
                  "blink.mojom.SerialService",
                  "blink.mojom.SharedWorkerConnector",
                  "blink.mojom.SmsReceiver",
                  "blink.mojom.SpeechRecognizer",
                  "blink.mojom.TextSuggestionHost",
                  "blink.mojom.UnhandledTapNotifier",
                  "blink.mojom.WakeLockService",
                  "blink.mojom.WebBluetoothService",
                  "blink.mojom.WebUsbService",
                  "content.mojom.BrowserTarget",
                  "content.mojom.InputInjector",
                  "content.mojom.RendererAudioInputStreamFactory",
                  "content.mojom.RendererAudioOutputStreamFactory",
                  "device.mojom.GamepadMonitor",
                  "device.mojom.Geolocation",
                  "device.mojom.NFC",
                  "device.mojom.SensorProvider",
                  "device.mojom.VibrationManager",
                  "device.mojom.VRService",
                  "discardable_memory.mojom.DiscardableSharedMemoryManager",
                  "media.mojom.ImageCapture",
                  "media.mojom.InterfaceFactory",
                  "media.mojom.MediaMetricsProvider",
                  "media.mojom.RemoterFactory",
                  "media.mojom.Renderer",
                  "media.mojom.VideoDecodePerfHistory",
                  "mojom.ProcessInternalsHandler",
                  "network.mojom.RestrictedCookieManager",
                  "blink.mojom.WebSocketConnector",
                  "payments.mojom.PaymentManager",
                  "payments.mojom.PaymentRequest",
                  "resource_coordinator.mojom.DocumentCoordinationUnit",
                  "shape_detection.mojom.BarcodeDetectionProvider",
                  "shape_detection.mojom.FaceDetectionProvider",
                  "shape_detection.mojom.TextDetection",
                  "viz.mojom.Gpu"})
          .RequireInterfaceFilterCapability_Deprecated(
              mojom::kRendererServiceName, "navigation:frame", "browser")
          .PackageService(content::GetManifest())
          .PackageService(file::GetManifest())
          .Build()};
  return *manifest;
}

}  // namespace content
