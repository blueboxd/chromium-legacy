// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/xr_runtime_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/trace_event/common/trace_event_common.h"
#include "build/build_config.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_features.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/orientation/orientation_device_provider.h"
#include "device/vr/vr_device_provider.h"

#if BUILDFLAG(ENABLE_OPENVR)
#include "device/vr/openvr/openvr_device.h"
#endif

#if defined(OS_ANDROID)
#include "device/vr/android/gvr/gvr_device_provider.h"

#if BUILDFLAG(ENABLE_ARCORE)
#include "device/vr/android/arcore/arcore_device_provider_factory.h"
#endif  // BUILDFLAG(ENABLE_ARCORE)

#else  // !defined(OS_ANDROID)
#include "chrome/browser/vr/service/isolated_device_provider.h"
#endif  // defined(OS_ANDROID)

namespace vr {

namespace {
XRRuntimeManager* g_xr_runtime_manager = nullptr;

base::LazyInstance<base::ObserverList<XRRuntimeManagerObserver>>::Leaky
    g_xr_runtime_manager_observers;

// Returns true if any of the AR-related features are enabled.
bool AreArFeaturesEnabled() {
  return base::FeatureList::IsEnabled(features::kWebXrHitTest) ||
         base::FeatureList::IsEnabled(features::kWebXrPlaneDetection);
}

}  // namespace

scoped_refptr<XRRuntimeManager> XRRuntimeManager::GetOrCreateInstance() {
  if (g_xr_runtime_manager) {
    return base::WrapRefCounted(g_xr_runtime_manager);
  }

  // Register VRDeviceProviders for the current platform
  ProviderList providers;

  if (AreArFeaturesEnabled()) {
#if defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_ARCORE)
    providers.emplace_back(device::ArCoreDeviceProviderFactory::Create());
#endif  // BUILDFLAG(ENABLE_ARCORE)
#endif  // defined(OS_ANDROID)
  }

#if defined(OS_ANDROID)
  providers.emplace_back(std::make_unique<device::GvrDeviceProvider>());
#else   // !defined(OS_ANDROID)
  providers.emplace_back(std::make_unique<vr::IsolatedVRDeviceProvider>());
#endif  // defined(OS_ANDROID)

  auto* connector = content::GetSystemConnector();
  if (connector) {
    providers.emplace_back(
        std::make_unique<device::VROrientationDeviceProvider>(connector));
  }

  return CreateInstance(std::move(providers));
}

bool XRRuntimeManager::HasInstance() {
  return g_xr_runtime_manager != nullptr;
}

XRRuntimeManager* XRRuntimeManager::GetInstanceIfCreated() {
  return g_xr_runtime_manager;
}

void XRRuntimeManager::RecordVrStartupHistograms() {
#if BUILDFLAG(ENABLE_OPENVR)
  device::OpenVRDevice::RecordRuntimeAvailability();
#endif
}

void XRRuntimeManager::AddObserver(XRRuntimeManagerObserver* observer) {
  g_xr_runtime_manager_observers.Get().AddObserver(observer);
}

void XRRuntimeManager::RemoveObserver(XRRuntimeManagerObserver* observer) {
  g_xr_runtime_manager_observers.Get().RemoveObserver(observer);
}

/* static */
void XRRuntimeManager::ExitImmersivePresentation() {
  if (!g_xr_runtime_manager) {
    return;
  }

  auto* browser_xr_runtime = g_xr_runtime_manager->GetImmersiveRuntime();
  if (!browser_xr_runtime) {
    return;
  }

  browser_xr_runtime->ExitVrFromPresentingRendererDevice();
}

void XRRuntimeManager::AddService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Loop through any currently active devices and send Connected messages to
  // the service. Future devices that come online will send a Connected message
  // when they are created.
  InitializeProviders();

  if (AreAllProvidersInitialized())
    service->InitializationComplete();

  services_.insert(service);
}

void XRRuntimeManager::RemoveService(VRServiceImpl* service) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  services_.erase(service);
}

BrowserXRRuntime* XRRuntimeManager::GetRuntime(device::mojom::XRDeviceId id) {
  auto it = runtimes_.find(id);
  if (it == runtimes_.end())
    return nullptr;

  return it->second.get();
}

BrowserXRRuntime* XRRuntimeManager::GetRuntimeForOptions(
    device::mojom::XRSessionOptions* options) {
  // Examine options to determine which device provider we should use.

  // AR requested.
  if (options->environment_integration) {
    if (!options->immersive) {
      DVLOG(1) << __func__ << ": non-immersive AR mode is unsupported";
      return nullptr;
    }
    // Return the ARCore device.
    return GetRuntime(device::mojom::XRDeviceId::ARCORE_DEVICE_ID);
  }

  if (options->immersive) {
    return GetImmersiveRuntime();
  } else {
    // Non immersive session.
    // Try the orientation provider if it exists.
    auto* orientation_runtime =
        GetRuntime(device::mojom::XRDeviceId::ORIENTATION_DEVICE_ID);
    if (orientation_runtime) {
      return orientation_runtime;
    }

    // If we don't have an orientation provider, then we don't have an explicit
    // runtime to back a non-immersive session
    return nullptr;
  }
}

BrowserXRRuntime* XRRuntimeManager::GetImmersiveRuntime() {
#if defined(OS_ANDROID)
  auto* gvr = GetRuntime(device::mojom::XRDeviceId::GVR_DEVICE_ID);
  if (gvr)
    return gvr;
#endif

#if BUILDFLAG(ENABLE_OPENXR)
  auto* openxr = GetRuntime(device::mojom::XRDeviceId::OPENXR_DEVICE_ID);
  if (openxr)
    return openxr;
#endif

#if BUILDFLAG(ENABLE_OPENVR)
  auto* openvr = GetRuntime(device::mojom::XRDeviceId::OPENVR_DEVICE_ID);
  if (openvr)
    return openvr;
#endif

#if BUILDFLAG(ENABLE_OCULUS_VR)
  auto* oculus = GetRuntime(device::mojom::XRDeviceId::OCULUS_DEVICE_ID);
  if (oculus)
    return oculus;
#endif

#if BUILDFLAG(ENABLE_WINDOWS_MR)
  auto* wmr = GetRuntime(device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID);
  if (wmr)
    return wmr;
#endif

  return nullptr;
}

device::mojom::VRDisplayInfoPtr XRRuntimeManager::GetCurrentVRDisplayInfo(
    XRDeviceImpl* device) {
  DVLOG(1) << __func__;
  // Get an immersive_runtime device if there is one.
  auto* immersive_runtime = GetImmersiveRuntime();
  if (immersive_runtime) {
    // Listen to changes for this device.
    immersive_runtime->OnRendererDeviceAdded(device);

    // If we don't have display info for the immersive device, get display info
    // from a different device.
    if (!immersive_runtime->GetVRDisplayInfo()) {
      immersive_runtime = nullptr;
    }
  }

  // Get an AR device if there is one.
  device::mojom::XRSessionOptions options = {};
  options.environment_integration = true;
  auto* ar_runtime = GetRuntimeForOptions(&options);
  if (ar_runtime) {
    // Listen to  changes for this device.
    ar_runtime->OnRendererDeviceAdded(device);
  }

  // If there is neither, use the generic non-immersive device.
  if (!ar_runtime && !immersive_runtime) {
    device::mojom::XRSessionOptions options = {};
    auto* non_immersive_runtime = GetRuntimeForOptions(&options);
    if (non_immersive_runtime) {
      // Listen to changes for this device.
      non_immersive_runtime->OnRendererDeviceAdded(device);
    }

    // If we don't have an AR or immersive device, return the generic non-
    // immersive device's DisplayInfo if we have it.
    return non_immersive_runtime ? non_immersive_runtime->GetVRDisplayInfo()
                                 : nullptr;
  }

  // Use the immersive or AR device.
  device::mojom::VRDisplayInfoPtr device_info =
      immersive_runtime ? immersive_runtime->GetVRDisplayInfo()
                        : ar_runtime->GetVRDisplayInfo();

  return device_info;
}

void XRRuntimeManager::OnRendererDeviceRemoved(XRDeviceImpl* device) {
  for (const auto& runtime : runtimes_) {
    runtime.second->OnRendererDeviceRemoved(device);
  }
}

bool XRRuntimeManager::IsOtherDevicePresenting(XRDeviceImpl* device) {
  DCHECK(device);

  auto* runtime = GetImmersiveRuntime();
  if (!runtime)
    return false;  // No immersive runtime to be presenting.

  XRDeviceImpl* presenting_device = runtime->GetPresentingRendererDevice();
  if (!presenting_device)
    return false;  // No XRDeviceImpl is presenting.

  // True if some other XRDeviceImpl is presenting.
  return (presenting_device != device);
}

bool XRRuntimeManager::HasAnyRuntime() {
  return !runtimes_.empty();
}

void XRRuntimeManager::SupportsSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::XRDevice::SupportsSessionCallback callback) {
  auto* runtime = GetRuntimeForOptions(options.get());

  if (!runtime) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(http://crbug.com/842025): Pass supports session on to the device
  // runtimes.
  std::move(callback).Run(true);
}

XRRuntimeManager::XRRuntimeManager(ProviderList providers)
    : providers_(std::move(providers)) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!g_xr_runtime_manager);
  g_xr_runtime_manager = this;
}

XRRuntimeManager::~XRRuntimeManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK_EQ(g_xr_runtime_manager, this);
  g_xr_runtime_manager = nullptr;
}

scoped_refptr<XRRuntimeManager> XRRuntimeManager::CreateInstance(
    ProviderList providers) {
  auto* ptr = new XRRuntimeManager(std::move(providers));
  CHECK_EQ(ptr, g_xr_runtime_manager);
  return base::AdoptRef(ptr);
}

device::mojom::XRRuntime* XRRuntimeManager::GetRuntimeForTest(
    device::mojom::XRDeviceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DeviceRuntimeMap::iterator iter =
      runtimes_.find(static_cast<device::mojom::XRDeviceId>(id));
  if (iter == runtimes_.end())
    return nullptr;

  return iter->second->GetRuntime();
}

size_t XRRuntimeManager::NumberOfConnectedServices() {
  return services_.size();
}

void XRRuntimeManager::InitializeProviders() {
  if (providers_initialized_)
    return;

  for (const auto& provider : providers_) {
    provider->Initialize(
        base::BindRepeating(&XRRuntimeManager::AddRuntime,
                            base::Unretained(this)),
        base::BindRepeating(&XRRuntimeManager::RemoveRuntime,
                            base::Unretained(this)),
        base::BindOnce(&XRRuntimeManager::OnProviderInitialized,
                       base::Unretained(this)));
  }

  providers_initialized_ = true;
}

void XRRuntimeManager::OnProviderInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ++num_initialized_providers_;
  if (AreAllProvidersInitialized()) {
    for (VRServiceImpl* service : services_)
      service->InitializationComplete();
  }
}

bool XRRuntimeManager::AreAllProvidersInitialized() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return num_initialized_providers_ == providers_.size();
}

void XRRuntimeManager::AddRuntime(device::mojom::XRDeviceId id,
                                  device::mojom::VRDisplayInfoPtr info,
                                  device::mojom::XRRuntimePtr runtime) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(runtimes_.find(id) == runtimes_.end());

  TRACE_EVENT_INSTANT1("xr", "AddRuntime", TRACE_EVENT_SCOPE_THREAD, "id", id);

  runtimes_[id] = std::make_unique<BrowserXRRuntime>(id, std::move(runtime),
                                                     std::move(info));

  for (XRRuntimeManagerObserver& obs : g_xr_runtime_manager_observers.Get())
    obs.OnRuntimeAdded(runtimes_[id].get());

  for (VRServiceImpl* service : services_)
    // TODO(sumankancherla): Consider combining with XRRuntimeManagerObserver.
    service->RuntimesChanged();
}

void XRRuntimeManager::RemoveRuntime(device::mojom::XRDeviceId id) {
  TRACE_EVENT_INSTANT1("xr", "RemoveRuntime", TRACE_EVENT_SCOPE_THREAD, "id",
                       id);

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = runtimes_.find(id);
  DCHECK(it != runtimes_.end());

  // Remove the device from runtimes_ before notifying services that it was
  // removed, since they will query for devices in RuntimesChanged.
  std::unique_ptr<BrowserXRRuntime> removed_device = std::move(it->second);
  runtimes_.erase(it);

  for (VRServiceImpl* service : services_)
    service->RuntimesChanged();
}

}  // namespace vr
