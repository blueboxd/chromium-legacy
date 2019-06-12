// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_

#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/ref_counted_video_source_provider.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"

namespace content {

// Implementation of VideoCaptureProvider that uses the "video_capture" service.
// Connects to the service lazily on demand and disconnects from the service as
// soon as all previously handed out VideoCaptureDeviceLauncher instances have
// been released and no more answers to GetDeviceInfosAsync() calls are pending.
// It is legal to create instances using |nullptr| as |connector| but for
// instances produced this way, it is illegal to subsequently call any of the
// public methods.
class CONTENT_EXPORT ServiceVideoCaptureProvider
    : public VideoCaptureProvider,
      public service_manager::mojom::ServiceManagerListener {
 public:
  // This constructor uses a default factory for instances of
  // viz::mojom::Gpu which produces instances of class content::GpuClient.
  ServiceVideoCaptureProvider(
      service_manager::Connector* connector,
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);

#if defined(OS_CHROMEOS)
  using CreateAcceleratorFactoryCallback = base::RepeatingCallback<
      std::unique_ptr<video_capture::mojom::AcceleratorFactory>()>;
  // Lets clients provide a custom mojo::Connector and factory method for
  // creating instances of viz::mojom::Gpu.
  ServiceVideoCaptureProvider(
      CreateAcceleratorFactoryCallback create_accelerator_factory_cb,
      service_manager::Connector* connector,
      base::RepeatingCallback<void(const std::string&)> emit_log_message_cb);
#endif  // defined(OS_CHROMEOS)

  ~ServiceVideoCaptureProvider() override;

  // VideoCaptureProvider implementation.
  void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) override;
  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override;

  // service_manager::mojom::ServiceManagerListener implementation.
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  running_services) override {}
  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr service) override {}
  void OnServiceStarted(const ::service_manager::Identity& identity,
                        uint32_t pid) override;
  void OnServicePIDReceived(const ::service_manager::Identity& identity,
                            uint32_t pid) override {}
  void OnServiceFailedToStart(
      const ::service_manager::Identity& identity) override {}
  void OnServiceStopped(const ::service_manager::Identity& identity) override;

 private:
  enum class ReasonForDisconnect { kShutdown, kUnused, kConnectionLost };

  void RegisterServiceListenerOnIOThread();
  // Note, this needs to have void return value because of "weak_ptrs can only
  // bind to methods without return values".
  void OnLauncherConnectingToSourceProvider(
      scoped_refptr<RefCountedVideoSourceProvider>* out_provider);
  // Discarding the returned RefCountedVideoSourceProvider indicates that the
  // caller no longer requires the connection to the service and allows it to
  // disconnect.
  scoped_refptr<RefCountedVideoSourceProvider> LazyConnectToService()
      WARN_UNUSED_RESULT;

  void GetDeviceInfosAsyncForRetry(GetDeviceInfosCallback result_callback,
                                   int retry_count);
  void OnDeviceInfosReceived(
      scoped_refptr<RefCountedVideoSourceProvider> service_connection,
      GetDeviceInfosCallback result_callback,
      int retry_count,
      const std::vector<media::VideoCaptureDeviceInfo>& infos);
  void OnDeviceInfosRequestDropped(
      scoped_refptr<RefCountedVideoSourceProvider> service_connection,
      GetDeviceInfosCallback result_callback,
      int retry_count);
  void OnLostConnectionToSourceProvider();
  void OnServiceConnectionClosed(ReasonForDisconnect reason);

  std::unique_ptr<service_manager::Connector> connector_;
#if defined(OS_CHROMEOS)
  CreateAcceleratorFactoryCallback create_accelerator_factory_cb_;
#endif  // defined(OS_CHROMEOS)
  base::RepeatingCallback<void(const std::string&)> emit_log_message_cb_;

  base::WeakPtr<RefCountedVideoSourceProvider> weak_service_connection_;

  bool launcher_has_connected_to_source_provider_;
  base::TimeTicks time_of_last_connect_;
  base::TimeTicks time_of_last_uninitialize_;

  mojo::Binding<service_manager::mojom::ServiceManagerListener>
      service_listener_binding_;

#if defined(OS_MACOSX)
  GetDeviceInfosCallback stashed_result_callback_for_retry_;
  int stashed_retry_count_;
#endif

  base::WeakPtrFactory<ServiceVideoCaptureProvider> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SERVICE_VIDEO_CAPTURE_PROVIDER_H_
