// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/fake_nearby_share_certificate_storage.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/client/fake_nearby_share_client.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/scheduling/fake_nearby_share_scheduler_factory.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const base::Time t0 =
    base::Time::UnixEpoch() + base::TimeDelta::FromDays(365 * 50);

const char kPageTokenPrefix[] = "page_token_";
const char kSecretIdPrefix[] = "secret_id_";
const char kDeviceIdPrefix[] = "users/me/devices/";
const char kDeviceId[] = "123456789A";

const std::vector<std::string> kPublicCertificateIds = {"id1", "id2", "id3"};

void CaptureDecryptedPublicCertificateCallback(
    base::Optional<NearbyShareDecryptedPublicCertificate>* dest,
    base::Optional<NearbyShareDecryptedPublicCertificate> src) {
  *dest = std::move(src);
}

}  // namespace

class NearbyShareCertificateManagerImplTest : public ::testing::Test {
 public:
  NearbyShareCertificateManagerImplTest() {
    local_device_data_manager_ =
        std::make_unique<FakeNearbyShareLocalDeviceDataManager>();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kNearbySharingSchedulerDownloadPublicCertificatesPrefName);

    NearbyShareSchedulerFactory::SetFactoryForTesting(&scheduler_factory_);
    NearbyShareCertificateStorageImpl::Factory::SetFactoryForTesting(
        &cert_store_factory_);

    cert_man_ = NearbyShareCertificateManagerImpl::Factory::Create(
        local_device_data_manager_.get(), pref_service_.get(),
        /*proto_database_provider=*/nullptr, base::FilePath(), &client_factory_,
        &clock_);

    scheduler_ =
        scheduler_factory_.pref_name_to_periodic_instance()
            .find(prefs::
                      kNearbySharingSchedulerDownloadPublicCertificatesPrefName)
            ->second.fake_scheduler;
    cert_store_ = cert_store_factory_.instances().back();

    PopulatePrivateCertificates();
    PopulatePublicCertificates();
  }

 protected:
  void GetPublicCertificatesCallback(
      bool success,
      const std::vector<nearbyshare::proto::PublicCertificate>& certs) {
    auto& callbacks = cert_store_->get_public_certificates_callbacks();
    auto callback = std::move(callbacks.back());
    callbacks.pop_back();
    auto pub_certs =
        std::make_unique<std::vector<nearbyshare::proto::PublicCertificate>>(
            certs.begin(), certs.end());
    std::move(callback).Run(success, std::move(pub_certs));
  }

  // Test downloading public certificates with or without errors. The RPC is
  // paginated, and |num_pages| will be simulated. If |rpc_fail| is not nullopt
  // or store_fail is true, then those respective failures will be simulated on
  // the last page.
  void DownloadPublicCertificatesFlow(
      size_t num_pages,
      base::Optional<NearbyShareHttpError> rpc_fail,
      bool store_fail) {
    size_t prev_num_results = scheduler_->handled_results().size();
    cert_store_->SetPublicCertificateIds(kPublicCertificateIds);
    local_device_data_manager_->SetId(kDeviceId);

    cert_man_->Start();
    scheduler_->InvokeRequestCallback();
    cert_man_->Stop();

    std::string page_token;
    for (size_t page_number = 0; page_number < num_pages; ++page_number) {
      bool last_page = page_number == num_pages - 1;

      auto& request = client_factory_.instances()
                          .back()
                          ->list_public_certificates_requests()
                          .back();
      CheckRpcRequest(request.request, page_token);

      if (last_page && rpc_fail) {
        std::move(request.error_callback).Run(*rpc_fail);
        break;
      }
      page_token = last_page
                       ? std::string()
                       : kPageTokenPrefix + base::NumberToString(page_number);
      std::move(request.callback)
          .Run(BuildRpcResponse(page_number, page_token));

      auto& add_cert_call = cert_store_->add_public_certificates_calls().back();
      CheckStorageAddCertificates(add_cert_call);
      std::move(add_cert_call.callback)
          .Run(/*success=*/!(last_page && store_fail));
    }
    ASSERT_EQ(scheduler_->handled_results().size(), prev_num_results + 1);
    EXPECT_EQ(scheduler_->handled_results().back(), !rpc_fail && !store_fail);
  }

  void CheckRpcRequest(
      const nearbyshare::proto::ListPublicCertificatesRequest& request,
      const std::string& page_token) {
    EXPECT_EQ(request.parent(), std::string(kDeviceIdPrefix) + kDeviceId);
    ASSERT_EQ(request.secret_ids_size(),
              static_cast<int>(kPublicCertificateIds.size()));
    for (size_t i = 0; i < kPublicCertificateIds.size(); ++i) {
      EXPECT_EQ(request.secret_ids(i), kPublicCertificateIds[i]);
    }
    EXPECT_EQ(request.page_token(), page_token);
  }

  nearbyshare::proto::ListPublicCertificatesResponse BuildRpcResponse(
      size_t page_number,
      const std::string& page_token) {
    nearbyshare::proto::ListPublicCertificatesResponse response;
    for (size_t i = 0; i < public_certificates_.size(); ++i) {
      public_certificates_[i].set_secret_id(kSecretIdPrefix +
                                            base::NumberToString(page_number) +
                                            "_" + base::NumberToString(i));
      response.add_public_certificates();
      *response.mutable_public_certificates(i) = public_certificates_[i];
    }
    response.set_next_page_token(page_token);
    return response;
  }

  void CheckStorageAddCertificates(
      const FakeNearbyShareCertificateStorage::AddPublicCertificatesCall&
          add_cert_call) {
    ASSERT_EQ(add_cert_call.public_certificates.size(),
              public_certificates_.size());
    for (size_t i = 0; i < public_certificates_.size(); ++i) {
      EXPECT_EQ(add_cert_call.public_certificates[i].secret_id(),
                public_certificates_[i].secret_id());
    }
  }

  void PopulatePrivateCertificates() {
    private_certificates_.clear();
    auto& metadata = GetNearbyShareTestMetadata();
    for (auto visibility : {NearbyShareVisibility::kAllContacts,
                            NearbyShareVisibility::kSelectedContacts}) {
      private_certificates_.emplace_back(visibility, t0, metadata);
      private_certificates_.emplace_back(
          visibility, t0 + kNearbyShareCertificateValidityPeriod, metadata);
      private_certificates_.emplace_back(
          visibility, t0 + kNearbyShareCertificateValidityPeriod * 2, metadata);
    }
  }

  void PopulatePublicCertificates() {
    public_certificates_.clear();
    metadata_encryption_keys_.clear();
    auto& metadata1 = GetNearbyShareTestMetadata();
    nearbyshare::proto::EncryptedMetadata metadata2;
    metadata2.set_device_name("device_name2");
    metadata2.set_full_name("full_name2");
    metadata2.set_icon_url("icon_url2");
    metadata2.set_bluetooth_mac_address("bluetooth_mac_address2");
    for (auto metadata : {metadata1, metadata2}) {
      auto private_cert = NearbySharePrivateCertificate(
          NearbyShareVisibility::kAllContacts, t0, metadata);
      public_certificates_.push_back(*private_cert.ToPublicCertificate());
      metadata_encryption_keys_.push_back(*private_cert.EncryptMetadataKey());
    }
  }

  std::unique_ptr<NearbyShareCertificateManager> cert_man_;
  std::unique_ptr<FakeNearbyShareLocalDeviceDataManager>
      local_device_data_manager_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  FakeNearbyShareCertificateStorage::Factory cert_store_factory_;
  FakeNearbyShareCertificateStorage* cert_store_;
  FakeNearbyShareClientFactory client_factory_;
  FakeNearbyShareSchedulerFactory scheduler_factory_;
  FakeNearbyShareScheduler* scheduler_;
  base::SimpleTestClock clock_;
  std::vector<NearbySharePrivateCertificate> private_certificates_;
  std::vector<nearbyshare::proto::PublicCertificate> public_certificates_;
  std::vector<NearbyShareEncryptedMetadataKey> metadata_encryption_keys_;
};

TEST_F(NearbyShareCertificateManagerImplTest, GetValidPrivateCertificate) {
  cert_store_->SetPrivateCertificates(private_certificates_);
  clock_.SetNow(t0 + kNearbyShareCertificateValidityPeriod * 1.5);
  auto cert = cert_man_->GetValidPrivateCertificate(
      NearbyShareVisibility::kAllContacts);

  EXPECT_EQ(NearbyShareVisibility::kAllContacts, cert.visibility());
  EXPECT_LE(cert.not_before(), clock_.Now());
  EXPECT_LT(clock_.Now(), cert.not_after());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateSuccess) {
  base::Optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_man_->GetDecryptedPublicCertificate(
      metadata_encryption_keys_[0],
      base::BindOnce(&CaptureDecryptedPublicCertificateCallback,
                     &decrypted_pub_cert));

  GetPublicCertificatesCallback(true, public_certificates_);

  ASSERT_TRUE(decrypted_pub_cert);
  std::vector<uint8_t> id(public_certificates_[0].secret_id().begin(),
                          public_certificates_[0].secret_id().end());
  EXPECT_EQ(decrypted_pub_cert->id(), id);
  EXPECT_EQ(decrypted_pub_cert->unencrypted_metadata().SerializeAsString(),
            GetNearbyShareTestMetadata().SerializeAsString());
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateCertNotFound) {
  auto private_cert = NearbySharePrivateCertificate(
      NearbyShareVisibility::kAllContacts, t0, GetNearbyShareTestMetadata());
  auto metadata_key = private_cert.EncryptMetadataKey();
  ASSERT_TRUE(metadata_key);

  base::Optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_man_->GetDecryptedPublicCertificate(
      *metadata_key, base::BindOnce(&CaptureDecryptedPublicCertificateCallback,
                                    &decrypted_pub_cert));

  GetPublicCertificatesCallback(true, public_certificates_);

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       GetDecryptedPublicCertificateGetPublicCertificatesFailure) {
  base::Optional<NearbyShareDecryptedPublicCertificate> decrypted_pub_cert;
  cert_man_->GetDecryptedPublicCertificate(
      metadata_encryption_keys_[0],
      base::BindOnce(&CaptureDecryptedPublicCertificateCallback,
                     &decrypted_pub_cert));

  GetPublicCertificatesCallback(false, {});

  EXPECT_FALSE(decrypted_pub_cert);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesImmediateRequest) {
  size_t prev_num_requests = scheduler_->num_immediate_requests();
  cert_man_->DownloadPublicCertificates();
  EXPECT_EQ(scheduler_->num_immediate_requests(), prev_num_requests + 1);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesSuccess) {
  DownloadPublicCertificatesFlow(/*num_pages=*/2, /*rpc_fail=*/base::nullopt,
                                 /*store_fail=*/false);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesRPCFailure) {
  DownloadPublicCertificatesFlow(
      /*num_pages=*/2, /*rpc_fail=*/NearbyShareHttpError::kResponseMalformed,
      /*store_fail=*/false);
}

TEST_F(NearbyShareCertificateManagerImplTest,
       DownloadPublicCertificatesStoreFailure) {
  DownloadPublicCertificatesFlow(/*num_pages=*/2, /*rpc_fail=*/base::nullopt,
                                 /*store_fail=*/true);
}
