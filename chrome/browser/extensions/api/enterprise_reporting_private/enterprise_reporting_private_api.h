// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "extensions/browser/extension_function.h"

namespace policy {
class CloudPolicyClient;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace extensions {
namespace enterprise_reporting {

extern const char kInvalidInputErrorMessage[];
extern const char kUploadFailed[];
extern const char kDeviceNotEnrolled[];
extern const char kDeviceIdNotFound[];

}  // namespace enterprise_reporting

class EnterpriseReportingPrivateUploadChromeDesktopReportFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "enterprise.reportingPrivate.uploadChromeDesktopReport",
      ENTERPRISEREPORTINGPRIVATE_UPLOADCHROMEDESKTOPREPORT)
  EnterpriseReportingPrivateUploadChromeDesktopReportFunction();

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  void SetCloudPolicyClientForTesting(
      std::unique_ptr<policy::CloudPolicyClient> client);
  void SetRegistrationInfoForTesting(const policy::DMToken& dm_token,
                                     const std::string& client_id);

  // Used by tests that want to overrode the URLLoaderFactory used to simulate
  // network requests.
  static EnterpriseReportingPrivateUploadChromeDesktopReportFunction*
  CreateForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  explicit EnterpriseReportingPrivateUploadChromeDesktopReportFunction(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~EnterpriseReportingPrivateUploadChromeDesktopReportFunction() override;

  // Callback once Chrome get the response from the DM Server.
  void OnReportUploaded(bool status);

  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  policy::DMToken dm_token_;
  std::string client_id_;

  DISALLOW_COPY_AND_ASSIGN(
      EnterpriseReportingPrivateUploadChromeDesktopReportFunction);
};

class EnterpriseReportingPrivateGetDeviceIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceId",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEID)
  EnterpriseReportingPrivateGetDeviceIdFunction();

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

 private:
  ~EnterpriseReportingPrivateGetDeviceIdFunction() override;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateGetDeviceIdFunction);
};

class EnterpriseReportingPrivateGetPersistentSecretFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getPersistentSecret",
                             ENTERPRISEREPORTINGPRIVATE_GETPERSISTENTSECRET)

  EnterpriseReportingPrivateGetPersistentSecretFunction();

 private:
  ~EnterpriseReportingPrivateGetPersistentSecretFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was retrieved from the file.
  void OnDataRetrieved(const std::string& data, bool status);

  DISALLOW_COPY_AND_ASSIGN(
      EnterpriseReportingPrivateGetPersistentSecretFunction);
};

class EnterpriseReportingPrivateGetDeviceDataFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceData",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEDATA)

  EnterpriseReportingPrivateGetDeviceDataFunction();

 private:
  ~EnterpriseReportingPrivateGetDeviceDataFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was retrieved from the file.
  void OnDataRetrieved(const std::string& data, bool status);

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateGetDeviceDataFunction);
};

class EnterpriseReportingPrivateSetDeviceDataFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.setDeviceData",
                             ENTERPRISEREPORTINGPRIVATE_SETDEVICEDATA)

  EnterpriseReportingPrivateSetDeviceDataFunction();

 private:
  ~EnterpriseReportingPrivateSetDeviceDataFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was stored to the file.
  void OnDataStored(bool status);

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateSetDeviceDataFunction);
};

class EnterpriseReportingPrivateGetDeviceInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceInfo",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEINFO)
  EnterpriseReportingPrivateGetDeviceInfoFunction();

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

 private:
  ~EnterpriseReportingPrivateGetDeviceInfoFunction() override;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateGetDeviceInfoFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_
