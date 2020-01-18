// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gem_device_details_manager.h"

#include <windows.h>
#include <winternl.h>

#include <lm.h>  // Needed for LSA_UNICODE_STRING
#include <process.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For POLICY_ALL_ACCESS types

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

const base::TimeDelta
    GemDeviceDetailsManager::kDefaultUploadDeviceDetailsRequestTimeout =
        base::TimeDelta::FromMilliseconds(12000);

// TODO(rakeshsoma): Change this to prod URL instead.
constexpr wchar_t kDefaultGemServiceUrl[] =
    L"https://autopush-gcpw-pa.sandbox.googleapis.com";

namespace {

// Constants used for contacting the gem service.
const char kGemServiceUploadDeviceDetailsPath[] = "/v1/uploadDeviceDetails";
const char kUploadDeviceDetailsRequestSerialNumberParameterName[] =
    "device_serial_number";
const char kUploadDeviceDetailsRequestMachineGuidParameterName[] =
    "machine_guid";

}  // namespace

// static
GemDeviceDetailsManager* GemDeviceDetailsManager::Get() {
  return *GetInstanceStorage();
}

// static
GemDeviceDetailsManager** GemDeviceDetailsManager::GetInstanceStorage() {
  static GemDeviceDetailsManager instance(
      kDefaultUploadDeviceDetailsRequestTimeout);
  static GemDeviceDetailsManager* instance_storage = &instance;
  return &instance_storage;
}

GemDeviceDetailsManager::GemDeviceDetailsManager(
    base::TimeDelta upload_device_details_request_timeout)
    : upload_device_details_request_timeout_(
          upload_device_details_request_timeout) {}

GemDeviceDetailsManager::~GemDeviceDetailsManager() = default;

GURL GemDeviceDetailsManager::GetGemServiceUploadDeviceDetailsUrl() {
  GURL gem_service_url = GURL(base::UTF16ToUTF8(kDefaultGemServiceUrl));

  return gem_service_url.Resolve(kGemServiceUploadDeviceDetailsPath);
}

// Uploads the device details into GEM database using |access_token| for
// authentication and authorization. The GEM service would use |serial_number|
// and |machine_guid| for identifying the device entry in GEM database.
// TODO(crbug.com/1043199): Store device_resource_id on device and send that to
// GEM service for further optimizations.
HRESULT GemDeviceDetailsManager::UploadDeviceDetails(
    const std::string& access_token) {
  base::string16 serial_number = GetSerialNumber();
  base::string16 machine_guid;
  HRESULT hr = GetMachineGuid(&machine_guid);

  base::Value request_dict(base::Value::Type::DICTIONARY);
  request_dict.SetStringKey(
      kUploadDeviceDetailsRequestSerialNumberParameterName,
      base::UTF16ToUTF8(serial_number));
  request_dict.SetStringKey(kUploadDeviceDetailsRequestMachineGuidParameterName,
                            base::UTF16ToUTF8(machine_guid));

  hr = WinHttpUrlFetcher::BuildRequestAndFetchResultFromHttpService(
      GemDeviceDetailsManager::Get()->GetGemServiceUploadDeviceDetailsUrl(),
      access_token, {}, request_dict, {},
      upload_device_details_request_timeout_);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "BuildRequestAndFetchResultFromHttpService hr="
                 << putHR(hr);
    return E_FAIL;
  }

  return hr;
}

}  // namespace credential_provider
