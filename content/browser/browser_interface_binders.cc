// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_interface_binders.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/background_fetch/background_fetch_service_impl.h"
#include "content/browser/content_index/content_index_service_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/image_capture/image_capture_impl.h"
#include "content/browser/keyboard_lock/keyboard_lock_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/screen_enumeration/screen_enumeration_impl.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/wake_lock/wake_lock_service_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/common/content_features.h"
#include "device/gamepad/gamepad_monitor.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"
#include "third_party/blink/public/mojom/credentialmanager/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom.h"

#if !defined(OS_ANDROID)
#include "content/browser/installedapp/installed_app_provider_impl_default.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#endif

#if defined(OS_ANDROID)
#include "services/device/public/mojom/nfc.mojom.h"
#endif

namespace content {
namespace internal {

// Forwards service receivers to Service Manager since the renderer cannot
// launch out-of-process services on is own.
template <typename Interface>
void ForwardServiceReceiver(const char* service_name,
                            RenderFrameHostImpl* host,
                            mojo::PendingReceiver<Interface> receiver) {
  auto* connector =
      BrowserContext::GetConnectorFor(host->GetProcess()->GetBrowserContext());
  connector->Connect(service_name, std::move(receiver));
}

// Documents/frames
void PopulateFrameBinders(RenderFrameHostImpl* host,
                          service_manager::BinderMap* map) {
  map->Add<blink::mojom::AppCacheBackend>(base::BindRepeating(
      &RenderFrameHostImpl::CreateAppCacheBackend, base::Unretained(host)));

  map->Add<blink::mojom::AudioContextManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetAudioContextManager, base::Unretained(host)));

  map->Add<blink::mojom::ContactsManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetContactsManager, base::Unretained(host)));

  map->Add<blink::mojom::FileSystemManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetFileSystemManager, base::Unretained(host)));

#if !defined(OS_ANDROID)
  map->Add<blink::mojom::HidService>(base::BindRepeating(
      &RenderFrameHostImpl::GetHidService, base::Unretained(host)));
#endif

  map->Add<blink::mojom::IdleManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetIdleManager, base::Unretained(host)));

#if !defined(OS_ANDROID)
  // The default (no-op) implementation of InstalledAppProvider. On Android, the
  // real implementation is provided in Java.
  map->Add<blink::mojom::InstalledAppProvider>(
      base::BindRepeating(&InstalledAppProviderImplDefault::Create));
#endif

  map->Add<blink::mojom::PermissionService>(base::BindRepeating(
      &RenderFrameHostImpl::CreatePermissionService, base::Unretained(host)));

  map->Add<blink::mojom::PresentationService>(base::BindRepeating(
      &RenderFrameHostImpl::GetPresentationService, base::Unretained(host)));

  map->Add<blink::mojom::SpeechSynthesis>(base::BindRepeating(
      &RenderFrameHostImpl::GetSpeechSynthesis, base::Unretained(host)));

  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));

  if (base::FeatureList::IsEnabled(features::kSmsReceiver)) {
    map->Add<blink::mojom::SmsReceiver>(base::BindRepeating(
        &RenderFrameHostImpl::BindSmsReceiverReceiver, base::Unretained(host)));
  }

  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &RenderFrameHostImpl::CreateLockManager, base::Unretained(host)));

  map->Add<blink::mojom::FileChooser>(base::BindRepeating(
      &RenderFrameHostImpl::GetFileChooser, base::Unretained(host)));

  map->Add<device::mojom::GamepadMonitor>(
      base::BindRepeating(&device::GamepadMonitor::Create));

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebNfc)) {
    map->Add<device::mojom::NFC>(base::BindRepeating(
        &RenderFrameHostImpl::BindNFCReceiver, base::Unretained(host)));
  }
#endif

  map->Add<device::mojom::SensorProvider>(base::BindRepeating(
      &RenderFrameHostImpl::GetSensorProvider, base::Unretained(host)));

  map->Add<device::mojom::VibrationManager>(base::BindRepeating(
      &ForwardServiceReceiver<device::mojom::VibrationManager>,
      device::mojom::kServiceName, base::Unretained(host)));

  map->Add<media::mojom::ImageCapture>(
      base::BindRepeating(&ImageCaptureImpl::Create));

  map->Add<payments::mojom::PaymentManager>(
      base::BindRepeating(&RenderProcessHost::CreatePaymentManager,
                          base::Unretained(host->GetProcess())));

  map->Add<blink::mojom::WebBluetoothService>(base::BindRepeating(
      &RenderFrameHostImpl::CreateWebBluetoothService, base::Unretained(host)));

  map->Add<blink::mojom::PushMessaging>(base::BindRepeating(
      &RenderFrameHostImpl::GetPushMessaging, base::Unretained(host)));

  map->Add<blink::mojom::CredentialManager>(base::BindRepeating(
      &RenderFrameHostImpl::GetCredentialManager, base::Unretained(host)));

  map->Add<blink::mojom::Authenticator>(base::BindRepeating(
      &RenderFrameHostImpl::GetAuthenticator, base::Unretained(host)));

  map->Add<blink::test::mojom::VirtualAuthenticatorManager>(
      base::BindRepeating(&RenderFrameHostImpl::GetVirtualAuthenticatorManager,
                          base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    RenderFrameHostImpl* host,
    service_manager::BinderMapWithContext<RenderFrameHost*>* map) {
  map->Add<blink::mojom::BackgroundFetchService>(
      base::BindRepeating(&BackgroundFetchServiceImpl::CreateForFrame));
  map->Add<blink::mojom::ContentIndexService>(
      base::BindRepeating(&ContentIndexServiceImpl::CreateForFrame));
  map->Add<blink::mojom::KeyboardLockService>(
      base::BindRepeating(&KeyboardLockServiceImpl::CreateMojoService));
  map->Add<blink::mojom::PictureInPictureService>(
      base::BindRepeating(&PictureInPictureServiceImpl::Create));
  map->Add<blink::mojom::WakeLockService>(
      base::BindRepeating(&WakeLockServiceImpl::Create));
  GetContentClient()->browser()->RegisterBrowserInterfaceBindersForFrame(map);
}

void PopulateBinderMap(RenderFrameHostImpl* host,
                       service_manager::BinderMap* map) {
  PopulateFrameBinders(host, map);
}

RenderFrameHost* GetContextForHost(RenderFrameHostImpl* host) {
  return host;
}

// Dedicated workers
const url::Origin& GetContextForHost(DedicatedWorkerHost* host) {
  return host->GetOrigin();
}

void PopulateDedicatedWorkerBinders(DedicatedWorkerHost* host,
                                    service_manager::BinderMap* map) {
  // base::Unretained(host) is safe because the map is owned by
  // |DedicatedWorkerHost::broker_|.
  map->Add<blink::mojom::FileSystemManager>(base::BindRepeating(
      &DedicatedWorkerHost::BindFileSystemManager, base::Unretained(host)));
  map->Add<blink::mojom::IdleManager>(base::BindRepeating(
      &DedicatedWorkerHost::CreateIdleManager, base::Unretained(host)));
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));
  if (base::FeatureList::IsEnabled(features::kSmsReceiver)) {
    map->Add<blink::mojom::SmsReceiver>(base::BindRepeating(
        &DedicatedWorkerHost::BindSmsReceiverReceiver, base::Unretained(host)));
  }
  map->Add<payments::mojom::PaymentManager>(base::BindRepeating(
      &DedicatedWorkerHost::CreatePaymentManager, base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    DedicatedWorkerHost* host,
    service_manager::BinderMapWithContext<const url::Origin&>* map) {
  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &RenderProcessHost::CreateLockManager,
      base::Unretained(host->GetProcessHost()), MSG_ROUTING_NONE));
  map->Add<blink::mojom::PermissionService>(
      base::BindRepeating(&RenderProcessHost::CreatePermissionService,
                          base::Unretained(host->GetProcessHost())));
}

void PopulateBinderMap(DedicatedWorkerHost* host,
                       service_manager::BinderMap* map) {
  PopulateDedicatedWorkerBinders(host, map);
}

// Shared workers
url::Origin GetContextForHost(SharedWorkerHost* host) {
  return url::Origin::Create(host->instance().url());
}

void PopulateSharedWorkerBinders(SharedWorkerHost* host,
                                 service_manager::BinderMap* map) {
  // base::Unretained(host) is safe because the map is owned by
  // |SharedWorkerHost::broker_|.
  map->Add<blink::mojom::AppCacheBackend>(base::BindRepeating(
      &SharedWorkerHost::CreateAppCacheBackend, base::Unretained(host)));
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));
  map->Add<payments::mojom::PaymentManager>(base::BindRepeating(
      &SharedWorkerHost::CreatePaymentManager, base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    SharedWorkerHost* host,
    service_manager::BinderMapWithContext<const url::Origin&>* map) {
  // TODO(https://crbug.com/873661): Pass origin to FileSystemManager.
  map->Add<blink::mojom::FileSystemManager>(
      base::BindRepeating(&RenderProcessHost::BindFileSystemManager,
                          base::Unretained(host->GetProcessHost())));
  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &RenderProcessHost::CreateLockManager,
      base::Unretained(host->GetProcessHost()), MSG_ROUTING_NONE));
  map->Add<blink::mojom::PermissionService>(
      base::BindRepeating(&RenderProcessHost::CreatePermissionService,
                          base::Unretained(host->GetProcessHost())));
}

void PopulateBinderMap(SharedWorkerHost* host,
                       service_manager::BinderMap* map) {
  PopulateSharedWorkerBinders(host, map);
}

// Service workers
ServiceWorkerVersionInfo GetContextForHost(ServiceWorkerProviderHost* host) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  return host->running_hosted_version()->GetInfo();
}

void PopulateServiceWorkerBinders(ServiceWorkerProviderHost* host,
                                  service_manager::BinderMap* map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  map->Add<blink::mojom::ScreenEnumeration>(
      base::BindRepeating(&ScreenEnumerationImpl::Create));

  map->Add<blink::mojom::LockManager>(base::BindRepeating(
      &ServiceWorkerProviderHost::CreateLockManager, base::Unretained(host)));

  map->Add<blink::mojom::PermissionService>(
      base::BindRepeating(&ServiceWorkerProviderHost::CreatePermissionService,
                          base::Unretained(host)));
  map->Add<payments::mojom::PaymentManager>(
      base::BindRepeating(&ServiceWorkerProviderHost::CreatePaymentManager,
                          base::Unretained(host)));
}

void PopulateBinderMapWithContext(
    ServiceWorkerProviderHost* host,
    service_manager::BinderMapWithContext<const ServiceWorkerVersionInfo&>*
        map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // Use a task runner if ServiceWorkerProviderHost lives on the IO
  // thread, as CreateForWorker() needs to be called on the UI thread.
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    map->Add<blink::mojom::BackgroundFetchService>(
        base::BindRepeating(&BackgroundFetchServiceImpl::CreateForWorker));
    map->Add<blink::mojom::ContentIndexService>(
        base::BindRepeating(&ContentIndexServiceImpl::CreateForWorker));
  } else {
    map->Add<blink::mojom::BackgroundFetchService>(
        base::BindRepeating(&BackgroundFetchServiceImpl::CreateForWorker),
        base::CreateSingleThreadTaskRunner(BrowserThread::UI));
    map->Add<blink::mojom::ContentIndexService>(
        base::BindRepeating(&ContentIndexServiceImpl::CreateForWorker),
        base::CreateSingleThreadTaskRunner(BrowserThread::UI));
  }
}

void PopulateBinderMap(ServiceWorkerProviderHost* host,
                       service_manager::BinderMap* map) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  PopulateServiceWorkerBinders(host, map);
}

}  // namespace internal
}  // namespace content
