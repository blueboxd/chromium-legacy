// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace chromeos {
namespace cert_provisioning {

class CertProvisioningScheduler;

class CertificateProvisioningUiHandler : public content::WebUIMessageHandler {
 public:
  CertificateProvisioningUiHandler();

  CertificateProvisioningUiHandler(
      const CertificateProvisioningUiHandler& other) = delete;
  CertificateProvisioningUiHandler& operator=(
      const CertificateProvisioningUiHandler& other) = delete;

  ~CertificateProvisioningUiHandler() override;

  // content::WebUIMessageHandler.
  void RegisterMessages() override;

 private:
  // Returns the per-user CertProvisioningScheduler for |user_profile|, if it
  // has any.
  chromeos::cert_provisioning::CertProvisioningScheduler*
  GetCertProvisioningSchedulerForUser(Profile* user_profile);

  // Returns the per-device CertProvisioningScheduler, if |user_profile| is
  // associated with a user that has access to device-wide client certificates.
  chromeos::cert_provisioning::CertProvisioningScheduler*
  GetCertProvisioningSchedulerForDevice(Profile* user_profile);

  // Send the list of certificate provisioning processes to the UI, triggered by
  // the UI when it loads.
  // |args| is expected to be empty.
  void HandleRefreshCertificateProvisioningProcesses(
      const base::ListValue* args);

  // Trigger an update / refresh on a certificate provisioning process.
  // |args| is expected to contain two arguments:
  // The argument at index 0 is a string specifying the certificate profile id
  // of the process that an update should be triggered for. The argument at
  // index 1 is a boolean specifying whether the process is a user-specific
  // (false) or a device-wide (true) certificate provisioning process.
  void HandleTriggerCertificateProvisioningProcessUpdate(
      const base::ListValue* args);

  // Send the list of certificate provisioning processes to the UI.
  void RefreshCertificateProvisioningProcesses();

  base::WeakPtrFactory<CertificateProvisioningUiHandler> weak_ptr_factory_{
      this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_
