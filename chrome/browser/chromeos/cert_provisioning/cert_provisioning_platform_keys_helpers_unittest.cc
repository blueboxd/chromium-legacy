// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_platform_keys_helpers.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_test_helpers.h"
#include "chrome/browser/chromeos/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Key;

namespace chromeos {
namespace cert_provisioning {
namespace {

template <typename MapType>
base::flat_set<typename MapType::key_type> GetKeys(const MapType& map) {
  base::flat_set<typename MapType::key_type> keys;
  for (const auto& pair : map) {
    keys.insert(pair.first);
  }
  return keys;
}

class CallbackObserver {
 public:
  using CertMap =
      base::flat_map<CertProfileId, scoped_refptr<net::X509Certificate>>;

  GetCertsWithIdsCallback GetCallback() {
    return base::BindOnce(&CallbackObserver::Callback, base::Unretained(this));
  }

  const CertMap& GetMap() { return cert_map_; }
  const std::string GetError() { return error_message_; }

  void WaitForCallback() { loop_.Run(); }

 protected:
  void Callback(CertMap certs_with_ids, const std::string& error_message) {
    cert_map_ = std::move(certs_with_ids);
    error_message_ = error_message;
    loop_.Quit();
  }

  base::RunLoop loop_;
  CertMap cert_map_;
  std::string error_message_;
};

class CertProvisioningCertsWithIdsGetterTest : public ::testing::Test {
 public:
  CertProvisioningCertsWithIdsGetterTest()
      : certificate_helper_(&platform_keys_service_) {}
  CertProvisioningCertsWithIdsGetterTest(
      const CertProvisioningCertsWithIdsGetterTest&) = delete;
  CertProvisioningCertsWithIdsGetterTest& operator=(
      const CertProvisioningCertsWithIdsGetterTest&) = delete;
  ~CertProvisioningCertsWithIdsGetterTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ProfileHelperForTesting profile_helper_;
  platform_keys::MockPlatformKeysService platform_keys_service_;
  CertificateHelperForTesting certificate_helper_;
};

TEST_F(CertProvisioningCertsWithIdsGetterTest, NoCertificates) {
  CertScope cert_scope = CertScope::kDevice;

  CertProvisioningCertsWithIdsGetter cert_getter;

  CallbackObserver callback_observer;
  cert_getter.GetCertsWithIds(cert_scope, &platform_keys_service_,
                              callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_TRUE(callback_observer.GetMap().empty());
  EXPECT_TRUE(callback_observer.GetError().empty());
}

TEST_F(CertProvisioningCertsWithIdsGetterTest, SingleCertificateWithId) {
  CertScope cert_scope = CertScope::kDevice;
  const char kCertProfileId[] = "cert_profile_id_1";

  certificate_helper_.AddCert(cert_scope, kCertProfileId);

  CertProvisioningCertsWithIdsGetter cert_getter;

  CallbackObserver callback_observer;
  cert_getter.GetCertsWithIds(cert_scope, &platform_keys_service_,
                              callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_THAT(GetKeys(callback_observer.GetMap()), ElementsAre(kCertProfileId));
  EXPECT_TRUE(callback_observer.GetError().empty());
}

TEST_F(CertProvisioningCertsWithIdsGetterTest, ManyCertificatesWithId) {
  CertScope cert_scope = CertScope::kDevice;
  std::vector<std::string> ids{"cert_profile_id_0", "cert_profile_id_1",
                               "cert_profile_id_2"};

  for (const auto& id : ids) {
    certificate_helper_.AddCert(cert_scope, id);
  }

  CertProvisioningCertsWithIdsGetter cert_getter;

  CallbackObserver callback_observer;
  cert_getter.GetCertsWithIds(cert_scope, &platform_keys_service_,
                              callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_THAT(GetKeys(callback_observer.GetMap()), ElementsAreArray(ids));
  EXPECT_TRUE(callback_observer.GetError().empty());
}

TEST_F(CertProvisioningCertsWithIdsGetterTest, ManyCertificatesWithoutId) {
  CertScope cert_scope = CertScope::kDevice;
  size_t cert_count = 4;
  std::vector<std::string> ids{"cert_profile_id_0", "cert_profile_id_1",
                               "cert_profile_id_2"};
  for (size_t i = 0; i < cert_count; ++i) {
    certificate_helper_.AddCert(cert_scope, /*cert_profile_id=*/"",
                                /*error_message=*/"no id");
  }

  CertProvisioningCertsWithIdsGetter cert_getter;

  CallbackObserver callback_observer;
  cert_getter.GetCertsWithIds(cert_scope, &platform_keys_service_,
                              callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_TRUE(callback_observer.GetMap().empty());
  EXPECT_TRUE(callback_observer.GetError().empty());
}

TEST_F(CertProvisioningCertsWithIdsGetterTest, CertificatesWithAndWithoutIds) {
  CertScope cert_scope = CertScope::kDevice;

  size_t cert_without_id_count = 4;
  for (size_t i = 0; i < cert_without_id_count; ++i) {
    certificate_helper_.AddCert(cert_scope, /*cert_profile_id=*/"",
                                /*error_message=*/"no id");
  }

  std::vector<std::string> ids{"cert_profile_id_0", "cert_profile_id_1",
                               "cert_profile_id_2"};
  for (const auto& id : ids) {
    certificate_helper_.AddCert(cert_scope, id);
  }

  CertProvisioningCertsWithIdsGetter cert_getter;

  CallbackObserver callback_observer;
  cert_getter.GetCertsWithIds(cert_scope, &platform_keys_service_,
                              callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_THAT(GetKeys(callback_observer.GetMap()), ElementsAreArray(ids));
  EXPECT_TRUE(callback_observer.GetError().empty());
}

}  // namespace
}  // namespace cert_provisioning
}  // namespace chromeos
