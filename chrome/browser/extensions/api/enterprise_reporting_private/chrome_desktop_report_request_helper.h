// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CHROME_DESKTOP_REPORT_REQUEST_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CHROME_DESKTOP_REPORT_REQUEST_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/policy/proto/device_management_backend.pb.h"

class Profile;

namespace base {
class DictionaryValue;
}  // namespace base

namespace extensions {

// Transfer the input from Json file to protobuf. Return nullptr if the input
// is not valid.
std::unique_ptr<enterprise_management::ChromeDesktopReportRequest>
GenerateChromeDesktopReportRequest(const base::DictionaryValue& report,
                                   Profile* profile);

// Override the path where Endpoint Verification data is stored for tests.
void OverrideEndpointVerificationDirForTesting(const base::FilePath& path);

// Store the |data| associated with the identifier |id|. Calls |callback| on
// completion with true on success.
void StoreDeviceData(const std::string& id,
                     const std::unique_ptr<std::vector<uint8_t>> data,
                     base::OnceCallback<void(bool)> callback);

// Retrieves the data associated with the identifier |id|. Calls |callback| on
// completion with the data retrieved if the second parameter is true.
void RetrieveDeviceData(
    const std::string& id,
    base::OnceCallback<void(const std::string&, bool)> callback);

// Get the Endpoint Verification secret (symmetric key) for this system. If no
// password exists in the Registry then one is generated, stored in the
// Registry, and returned.
// If one exists then it is fetched from the Registry and returned.
// If an error occurs then the second parameter is false.
void RetrieveDeviceSecret(
    base::OnceCallback<void(const std::string&, bool)> callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CHROME_DESKTOP_REPORT_REQUEST_HELPER_H_
