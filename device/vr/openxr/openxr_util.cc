// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_util.h"
#include "device/vr/openxr/openxr_defs.h"

#include <d3d11.h>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/stl_util.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"

namespace device {

XrPosef PoseIdentity() {
  XrPosef pose{};
  pose.orientation.w = 1;
  return pose;
}

XrResult GetSystem(XrInstance instance, XrSystemId* system) {
  XrSystemGetInfo system_info = {XR_TYPE_SYSTEM_GET_INFO};
  system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  return xrGetSystem(instance, &system_info, system);
}

#if defined(OS_WIN)
bool IsRunningInWin32AppContainer() {
  base::win::ScopedHandle scopedProcessToken;
  HANDLE processToken;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &processToken)) {
    return false;
  }

  scopedProcessToken.Set(processToken);

  BOOL isAppContainer;
  DWORD dwSize = sizeof(BOOL);
  if (!GetTokenInformation(scopedProcessToken.Get(), TokenIsAppContainer,
                           &isAppContainer, dwSize, &dwSize)) {
    return false;
  }

  return isAppContainer;
}
#else
bool IsRunningInWin32AppContainer() {
  return false;
}
#endif

XrResult CreateInstance(XrInstance* instance,
                        OpenXRInstanceMetadata* metadata) {
  XrInstanceCreateInfo instance_create_info = {XR_TYPE_INSTANCE_CREATE_INFO};

  std::string application_name = version_info::GetProductName() + " " +
                                 version_info::GetMajorVersionNumber();
  errno_t error =
      strcpy_s(instance_create_info.applicationInfo.applicationName,
               base::size(instance_create_info.applicationInfo.applicationName),
               application_name.c_str());
  DCHECK_EQ(error, 0);

  base::Version version = version_info::GetVersion();
  DCHECK_EQ(version.components().size(), 4uLL);
  uint32_t build = version.components()[2];

  // application version will be the build number of each vendor
  instance_create_info.applicationInfo.applicationVersion = build;

  error = strcpy_s(instance_create_info.applicationInfo.engineName,
                   base::size(instance_create_info.applicationInfo.engineName),
                   "Chromium");
  DCHECK_EQ(error, 0);

  // engine version should be the build number of chromium
  instance_create_info.applicationInfo.engineVersion = build;

  instance_create_info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

  uint32_t extensionCount;
  RETURN_IF_XR_FAILED(xrEnumerateInstanceExtensionProperties(
      nullptr, 0, &extensionCount, nullptr));
  std::vector<XrExtensionProperties> extensionProperties(
      extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
  RETURN_IF_XR_FAILED(xrEnumerateInstanceExtensionProperties(
      nullptr, extensionCount, &extensionCount, extensionProperties.data()));

  // xrCreateInstance validates the list of extensions and returns
  // XR_ERROR_EXTENSION_NOT_PRESENT if an extension is not supported,
  // so we don't need to call xrEnumerateInstanceExtensionProperties
  // to validate these extensions.
  // Since the OpenXR backend only knows how to draw with D3D11 at the moment,
  // the XR_KHR_D3D11_ENABLE_EXTENSION_NAME is required.
  std::vector<const char*> extensions{XR_KHR_D3D11_ENABLE_EXTENSION_NAME};

  // If we are in an app container, we must require the app container extension
  // to ensure robust execution of the OpenXR runtime
  if (IsRunningInWin32AppContainer()) {
    // Add the win32 app container compatible extension to our list of
    // extensions. If this runtime does not support execution in an app
    // container environment, one of xrCreateInstance or xrGetSystem will fail.
    extensions.push_back(kWin32AppcontainerCompatibleExtensionName);
  }

  // XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME, is required for optional
  // functionality (unbounded reference spaces) and thus only requested if it is
  // available.
  auto extensionSupported = [&extensionProperties](const char* extensionName) {
    return std::find_if(
               extensionProperties.begin(), extensionProperties.end(),
               [&extensionName](const XrExtensionProperties& properties) {
                 return strcmp(properties.extensionName, extensionName) == 0;
               }) != extensionProperties.end();
  };

  const bool unboundedSpaceExtensionSupported =
      extensionSupported(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME);
  if (unboundedSpaceExtensionSupported) {
    extensions.push_back(XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME);
  }

  if (metadata != nullptr) {
    metadata->unboundedReferenceSpaceSupported =
        unboundedSpaceExtensionSupported;
  }

  instance_create_info.enabledExtensionCount =
      static_cast<uint32_t>(extensions.size());
  instance_create_info.enabledExtensionNames = extensions.data();

  return xrCreateInstance(&instance_create_info, instance);
}

}  // namespace device
