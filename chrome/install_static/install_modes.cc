// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_modes.h"

#include "chrome/install_static/buildflags.h"

namespace install_static {

namespace {

#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
std::wstring GetClientsKeyPathForApp(const wchar_t* app_guid) {
  return std::wstring(L"Software\\Google\\Update\\Clients\\").append(app_guid);
}

std::wstring GetClientStateKeyPathForApp(const wchar_t* app_guid) {
  return std::wstring(L"Software\\Google\\Update\\ClientState\\")
      .append(app_guid);
}

std::wstring GetClientStateMediumKeyPathForApp(const wchar_t* app_guid) {
  return std::wstring(L"Software\\Google\\Update\\ClientStateMedium\\")
      .append(app_guid);
}
#else
std::wstring GetUnregisteredKeyPathForProduct(const wchar_t* product) {
  return std::wstring(L"Software\\").append(product);
}
#endif

}  // namespace

std::wstring GetClientsKeyPath(const wchar_t* app_guid) {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct(kProductPathName);
#else
  return GetClientsKeyPathForApp(app_guid);
#endif
}

std::wstring GetClientStateKeyPath(const wchar_t* app_guid) {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct(kProductPathName);
#else
  return GetClientStateKeyPathForApp(app_guid);
#endif
}

std::wstring GetBinariesClientsKeyPath() {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct(kBinariesPathName);
#else
  return GetClientsKeyPathForApp(kBinariesAppGuid);
#endif
}

std::wstring GetBinariesClientStateKeyPath() {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct(kBinariesPathName);
#else
  return GetClientStateKeyPathForApp(kBinariesAppGuid);
#endif
}

std::wstring GetClientStateMediumKeyPath(const wchar_t* app_guid) {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct(kProductPathName);
#else
  return GetClientStateMediumKeyPathForApp(app_guid);
#endif
}

std::wstring GetBinariesClientStateMediumKeyPath() {
#if !BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  return GetUnregisteredKeyPathForProduct(kBinariesPathName);
#else
  return GetClientStateMediumKeyPathForApp(kBinariesAppGuid);
#endif
}

}  // namespace install_static
