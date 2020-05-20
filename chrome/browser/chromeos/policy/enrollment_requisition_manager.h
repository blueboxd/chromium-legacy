// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_REQUISITION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_REQUISITION_MANAGER_H_

#include <string>

class PrefRegistrySimple;
class PrefService;

namespace policy {

// Managed device requisition that is being stored in local state and is used
// during enrollment to specify the intended use of the device.
class EnrollmentRequisitionManager {
 public:
  EnrollmentRequisitionManager();
  ~EnrollmentRequisitionManager() = default;
  EnrollmentRequisitionManager(const EnrollmentRequisitionManager&) = delete;
  EnrollmentRequisitionManager* operator=(const EnrollmentRequisitionManager&) =
      delete;

  // Initializes requisition information.
  void Initialize(PrefService* local_state);

  // Gets/Sets the device requisition.
  std::string GetDeviceRequisition() const;
  void SetDeviceRequisition(const std::string& requisition);
  bool IsRemoraRequisition() const;
  bool IsSharkRequisition() const;

  // Gets/Sets the sub organization.
  std::string GetSubOrganization() const;
  void SetSubOrganization(const std::string& sub_organization);

  // If set, the device will start the enterprise enrollment OOBE.
  void SetDeviceEnrollmentAutoStart();

  // Pref registration helper.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Initializes requisition settings at OOBE with values from VPD.
  void InitializeRequisition();

  // PrefService instance to read the requisition from.
  PrefService* local_state_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_REQUISITION_MANAGER_H_
