// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_
#define CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_

#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "extensions/common/extension_id.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class GURL;
class Profile;

namespace base {
class FilePath;
class Version;
}  // namespace base

#if defined(OS_CHROMEOS)

namespace policy {
class DevicePolicyCrosTestHelper;
}  // namespace policy

#endif  // OS_CHROMEOS

// A mixin that allows to force-install an extension/app via the device policy.
//
// Encapsulates the following operations:
// * generating an update manifest,
// * hosting the update manifest and the CRX via an embedded test server,
// * configuring the force installation in the device policy.
//
// Example usage (for force-installing into the sign-in profile using the device
// policy):
//
//   class MyTestFixture : ... {
//    protected:
//     void SetUpOnMainThread() override {
//       ...
//       force_install_mixin_.InitWithDevicePolicyCrosTestHelper(...);
//     }
//     void ForceInstall() {
//       EXPECT_TRUE(force_install_mixin_.ForceInstallFromCrx(...));
//     }
//     ExtensionForceInstallMixin force_install_mixin_{&mixin_host_};
//   };
//
// TODO(crbug.com/1090941): Add user policy, CRX packing, awaiting, auto update.
class ExtensionForceInstallMixin final : public InProcessBrowserTestMixin {
 public:
  explicit ExtensionForceInstallMixin(InProcessBrowserTestMixinHost* host);
  ExtensionForceInstallMixin(const ExtensionForceInstallMixin&) = delete;
  ExtensionForceInstallMixin& operator=(const ExtensionForceInstallMixin&) =
      delete;
  ~ExtensionForceInstallMixin() override;

  // Use one of the Init*() methods to initialize the object before calling any
  // other method:

#if defined(OS_CHROMEOS)
  void InitWithDevicePolicyCrosTestHelper(
      Profile* profile,
      policy::DevicePolicyCrosTestHelper* device_policy_cros_test_helper);
#endif  // OS_CHROMEOS

  // Force-installs the CRX file |crx_path|; under the hood, generates an update
  // manifest and serves it and the CRX file by the embedded test server.
  // |extension_id| - if non-null, will be set to the installed extension ID.
  bool ForceInstallFromCrx(const base::FilePath& crx_path,
                           extensions::ExtensionId* extension_id = nullptr);

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

 private:
  // Returns the directory whose contents are served by the embedded test
  // server.
  base::FilePath GetServedDirPath() const;
  // Returns the URL of the update manifest pointing to the embedded test
  // server.
  GURL GetServedUpdateManifestUrl(
      const extensions::ExtensionId& extension_id) const;
  // Returns the URL of the CRX file pointing to the embedded test server.
  GURL GetServedCrxUrl(const extensions::ExtensionId& extension_id,
                       const base::Version& extension_version) const;
  // Makes the given |source_crx_path| file served by the embedded test server.
  bool ServeExistingCrx(const base::FilePath& source_crx_path,
                        const extensions::ExtensionId& extension_id,
                        const base::Version& extension_version);
  // Creates an update manifest with the CRX URL pointing to the embedded test
  // server.
  bool CreateAndServeUpdateManifestFile(
      const extensions::ExtensionId& extension_id,
      const base::Version& extension_version);
  // Sets the policy to force-install the given extension from the given update
  // manifest URL.
  bool UpdatePolicy(const extensions::ExtensionId& extension_id,
                    const GURL& update_manifest_url);

  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer embedded_test_server_;
  Profile* profile_ = nullptr;
#if defined(OS_CHROMEOS)
  policy::DevicePolicyCrosTestHelper* device_policy_cros_test_helper_ = nullptr;
#endif
};

#endif  // CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_
