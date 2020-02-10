// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GEM_DEVICE_DETAILS_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GEM_DEVICE_DETAILS_MANAGER_H_

#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/windows_types.h"
#include "url/gurl.h"

namespace credential_provider {

// Manager used to handle requests to store device details in GEM database.
class GemDeviceDetailsManager {
 public:
  // Default timeout when trying to make requests to the GEM cloud service
  // to upload device details from GCPW.
  static const base::TimeDelta kDefaultUploadDeviceDetailsRequestTimeout;

  static GemDeviceDetailsManager* Get();

  // Upload device details to gem database.
  HRESULT UploadDeviceDetails(const std::string& access_token,
                              const base::string16& sid,
                              const base::string16& username,
                              const base::string16& domain);

  // Set the upload device details http response status for the
  // purpose of unit testing.
  void SetUploadStatusForTesting(HRESULT hr) { upload_status_ = hr; }

  // Calculates the full url of various gem service requests.
  GURL GetGemServiceUploadDeviceDetailsUrl();

 protected:
  // Returns the storage used for the instance pointer.
  static GemDeviceDetailsManager** GetInstanceStorage();

  explicit GemDeviceDetailsManager(
      base::TimeDelta upload_device_details_request_timeout);
  virtual ~GemDeviceDetailsManager();

  // Sets the timeout of http request to the GEM Service for the
  // purposes of unit testing.
  void SetRequestTimeoutForTesting(base::TimeDelta request_timeout) {
    upload_device_details_request_timeout_ = request_timeout;
  }

  // Gets the request dictionary used to invoke the GEM service for
  // the purpose of testing.
  const base::Value& GetRequestDictForTesting() { return *request_dict_; }

  // Get the upload device details http response status for the
  // purpose of unit testing.
  HRESULT GetUploadStatusForTesting() { return upload_status_; }

 private:
  base::TimeDelta upload_device_details_request_timeout_;
  HRESULT upload_status_;
  std::unique_ptr<base::Value> request_dict_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GEM_DEVICE_DETAILS_MANAGER_H_
