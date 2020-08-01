// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage_impl.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kSecretId1[] = "secretid1";
const char kSecretKey1[] = "secretkey1";
const char kPublicKey1[] = "publickey1";
const int64_t kStartSeconds1 = 0;
const int32_t kStartNanos1 = 10;
const int64_t kEndSeconds1 = 100;
const int32_t kEndNanos1 = 30;
const bool kForSelectedContacts1 = false;
const char kMetadataEncryptionKey1[] = "metadataencryptionkey1";
const char kEncryptedMetadataBytes1[] = "encryptedmetadatabytes1";
const char kMetadataEncryptionKeyTag1[] = "metadataencryptionkeytag1";
const char kSecretId2[] = "secretid2";
const char kSecretKey2[] = "secretkey2";
const char kPublicKey2[] = "publickey2";
const int64_t kStartSeconds2 = 0;
const int32_t kStartNanos2 = 20;
const int64_t kEndSeconds2 = 200;
const int32_t kEndNanos2 = 30;
const bool kForSelectedContacts2 = false;
const char kMetadataEncryptionKey2[] = "metadataencryptionkey2";
const char kEncryptedMetadataBytes2[] = "encryptedmetadatabytes2";
const char kMetadataEncryptionKeyTag2[] = "metadataencryptionkeytag2";
const char kSecretId3[] = "secretid3";
const char kSecretKey3[] = "secretkey3";
const char kPublicKey3[] = "publickey3";
const int64_t kStartSeconds3 = 0;
const int32_t kStartNanos3 = 30;
const int64_t kEndSeconds3 = 300;
const int32_t kEndNanos3 = 30;
const bool kForSelectedContacts3 = false;
const char kMetadataEncryptionKey3[] = "metadataencryptionkey3";
const char kEncryptedMetadataBytes3[] = "encryptedmetadatabytes3";
const char kMetadataEncryptionKeyTag3[] = "metadataencryptionkeytag3";
const char kSecretId4[] = "secretid4";
const char kSecretKey4[] = "secretkey4";
const char kPublicKey4[] = "publickey4";
const int64_t kStartSeconds4 = 0;
const int32_t kStartNanos4 = 10;
const int64_t kEndSeconds4 = 100;
const int32_t kEndNanos4 = 30;
const bool kForSelectedContacts4 = false;
const char kMetadataEncryptionKey4[] = "metadataencryptionkey4";
const char kEncryptedMetadataBytes4[] = "encryptedmetadatabytes4";
const char kMetadataEncryptionKeyTag4[] = "metadataencryptionkeytag4";

const char kNearbySharePublicCertificateExpirationDictPref[] =
    "nearbyshare.public_certificate_expiration_dict";

std::string EncodeString(const std::string& unencoded_string) {
  std::string encoded_string;
  base::Base64UrlEncode(unencoded_string,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_string);
  return encoded_string;
}

nearbyshare::proto::PublicCertificate CreatePublicCertificate(
    const std::string& secret_id,
    const std::string& secret_key,
    const std::string& public_key,
    int64_t start_seconds,
    int32_t start_nanos,
    int64_t end_seconds,
    int32_t end_nanos,
    bool for_selected_contacts,
    const std::string& metadata_encryption_key,
    const std::string& encrypted_metadata_bytes,
    const std::string& metadata_encryption_key_tag) {
  nearbyshare::proto::PublicCertificate cert;
  cert.set_secret_id(secret_id);
  cert.set_secret_key(secret_key);
  cert.set_public_key(public_key);
  cert.mutable_start_time()->set_seconds(start_seconds);
  cert.mutable_start_time()->set_nanos(start_nanos);
  cert.mutable_end_time()->set_seconds(end_seconds);
  cert.mutable_end_time()->set_nanos(end_nanos);
  cert.set_for_selected_contacts(for_selected_contacts);
  cert.set_metadata_encryption_key(metadata_encryption_key);
  cert.set_encrypted_metadata_bytes(encrypted_metadata_bytes);
  cert.set_metadata_encryption_key_tag(metadata_encryption_key_tag);
  return cert;
}

std::vector<NearbySharePrivateCertificate> CreatePrivateCertificates(size_t n) {
  std::vector<NearbySharePrivateCertificate> certs;
  certs.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    certs.emplace_back(NearbyShareVisibility::kAllContacts, base::Time::Now(),
                       GetNearbyShareTestMetadata());
  }
  return certs;
}

base::Time TimestampToTime(nearbyshare::proto::Timestamp timestamp) {
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromSeconds(timestamp.seconds()) +
         base::TimeDelta::FromNanoseconds(timestamp.nanos());
}
}  // namespace

class NearbyShareCertificateStorageImplTest : public ::testing::Test {
 public:
  NearbyShareCertificateStorageImplTest() = default;
  ~NearbyShareCertificateStorageImplTest() override = default;
  NearbyShareCertificateStorageImplTest(
      NearbyShareCertificateStorageImplTest&) = delete;
  NearbyShareCertificateStorageImplTest& operator=(
      NearbyShareCertificateStorageImplTest&) = delete;

  void SetUp() override {
    auto db = std::make_unique<
        leveldb_proto::test::FakeDB<nearbyshare::proto::PublicCertificate>>(
        &db_entries_);
    db_ = db.get();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    NearbyShareCertificateStorageImpl::RegisterPrefs(pref_service_->registry());

    // Add public certificates to database before construction. Needed
    // to ensure test coverage of FetchPublicCertificateExpirations.
    PrepopulatePublicCertificates();

    cert_store_ = NearbyShareCertificateStorageImpl::Factory::Create(
        pref_service_.get(), std::move(db));
  }

  bool Initialize(leveldb_proto::Enums::InitStatus init_status) {
    // Make a fresh copy of cert_store_ to get back to uninitialized state.
    if (cert_store_->IsInitialized())
      SetUp();

    bool init_success = false;
    cert_store_->Initialize(base::BindOnce(
        &NearbyShareCertificateStorageImplTest::CaptureBoolCallback,
        base::Unretained(this), &init_success));
    db_->InitStatusCallback(init_status);
    return init_success;
  }

  void PrepopulatePublicCertificates() {
    std::vector<nearbyshare::proto::PublicCertificate> pub_certs;
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId1, kSecretKey1, kPublicKey1, kStartSeconds1, kStartNanos1,
        kEndSeconds1, kEndNanos1, kForSelectedContacts1,
        kMetadataEncryptionKey1, kEncryptedMetadataBytes1,
        kMetadataEncryptionKeyTag1));
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId2, kSecretKey2, kPublicKey2, kStartSeconds2, kStartNanos2,
        kEndSeconds2, kEndNanos2, kForSelectedContacts2,
        kMetadataEncryptionKey2, kEncryptedMetadataBytes2,
        kMetadataEncryptionKeyTag2));
    pub_certs.emplace_back(CreatePublicCertificate(
        kSecretId3, kSecretKey3, kPublicKey3, kStartSeconds3, kStartNanos3,
        kEndSeconds3, kEndNanos3, kForSelectedContacts3,
        kMetadataEncryptionKey3, kEncryptedMetadataBytes3,
        kMetadataEncryptionKeyTag3));

    base::Value expiration_dict(base::Value::Type::DICTIONARY);
    db_entries_.clear();
    for (auto& cert : pub_certs) {
      expiration_dict.SetKey(
          EncodeString(cert.secret_id()),
          util::TimeToValue(TimestampToTime(cert.end_time())));
      db_entries_.emplace(cert.secret_id(), std::move(cert));
    }
    pref_service_->Set(kNearbySharePublicCertificateExpirationDictPref,
                       expiration_dict);
  }

  void CaptureBoolCallback(bool* dest, bool src) { *dest = src; }

  void PublicCertificateCallback(
      std::vector<nearbyshare::proto::PublicCertificate>* public_certificates,
      bool success,
      std::unique_ptr<std::vector<nearbyshare::proto::PublicCertificate>>
          result) {
    if (success && result) {
      public_certificates->swap(*result);
    }
  }

 protected:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::map<std::string, nearbyshare::proto::PublicCertificate> db_entries_;
  leveldb_proto::test::FakeDB<nearbyshare::proto::PublicCertificate>* db_;
  std::unique_ptr<NearbyShareCertificateStorage> cert_store_;
  std::vector<nearbyshare::proto::PublicCertificate> public_certificates_;
};

TEST_F(NearbyShareCertificateStorageImplTest, InitializeSucceeded) {
  if (cert_store_->IsInitialized())
    SetUp();

  ASSERT_FALSE(cert_store_->IsInitialized());

  bool succeeded = Initialize(leveldb_proto::Enums::InitStatus::kOK);

  ASSERT_TRUE(cert_store_->IsInitialized());
  ASSERT_TRUE(succeeded);
}

TEST_F(NearbyShareCertificateStorageImplTest, InitializeFailed) {
  if (cert_store_->IsInitialized())
    SetUp();

  ASSERT_FALSE(cert_store_->IsInitialized());

  bool succeeded = Initialize(leveldb_proto::Enums::InitStatus::kError);

  ASSERT_FALSE(cert_store_->IsInitialized());
  ASSERT_FALSE(succeeded);
}

TEST_F(NearbyShareCertificateStorageImplTest, GetPublicCertificateIds) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  auto ids = cert_store_->GetPublicCertificateIds();
  ASSERT_EQ(3u, ids.size());
  EXPECT_EQ(ids[0], kSecretId1);
  EXPECT_EQ(ids[1], kSecretId2);
  EXPECT_EQ(ids[2], kSecretId3);
}

TEST_F(NearbyShareCertificateStorageImplTest, GetPublicCertificates) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  std::vector<nearbyshare::proto::PublicCertificate> public_certificates;
  cert_store_->GetPublicCertificates(base::BindOnce(
      &NearbyShareCertificateStorageImplTest::PublicCertificateCallback,
      base::Unretained(this), &public_certificates));
  db_->LoadCallback(true);

  ASSERT_EQ(3u, public_certificates.size());
  for (nearbyshare::proto::PublicCertificate& cert : public_certificates) {
    std::string expected_serialized, actual_serialized;
    ASSERT_TRUE(cert.SerializeToString(&expected_serialized));
    ASSERT_TRUE(db_entries_.find(cert.secret_id())
                    ->second.SerializeToString(&actual_serialized));
    ASSERT_EQ(expected_serialized, actual_serialized);
  }
}

TEST_F(NearbyShareCertificateStorageImplTest, ReplacePublicCertificates) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  std::vector<nearbyshare::proto::PublicCertificate> new_certs = {
      CreatePublicCertificate(kSecretId4, kSecretKey4, kPublicKey4,
                              kStartSeconds4, kStartNanos4, kEndSeconds4,
                              kEndNanos4, kForSelectedContacts4,
                              kMetadataEncryptionKey4, kEncryptedMetadataBytes4,
                              kMetadataEncryptionKeyTag4),
  };

  bool succeeded = false;
  cert_store_->ReplacePublicCertificates(
      new_certs,
      base::BindOnce(
          &NearbyShareCertificateStorageImplTest::CaptureBoolCallback,
          base::Unretained(this), &succeeded));
  db_->DestroyCallback(true);
  db_->UpdateCallback(true);

  ASSERT_TRUE(succeeded);
  ASSERT_EQ(1u, db_entries_.size());
  ASSERT_EQ(1u, db_entries_.count(kSecretId4));
  auto& cert = db_entries_.find(kSecretId4)->second;
  EXPECT_EQ(kSecretKey4, cert.secret_key());
  EXPECT_EQ(kPublicKey4, cert.public_key());
  EXPECT_EQ(kStartSeconds4, cert.start_time().seconds());
  EXPECT_EQ(kStartNanos4, cert.start_time().nanos());
  EXPECT_EQ(kEndSeconds4, cert.end_time().seconds());
  EXPECT_EQ(kEndNanos4, cert.end_time().nanos());
  EXPECT_EQ(kForSelectedContacts4, cert.for_selected_contacts());
  EXPECT_EQ(kMetadataEncryptionKey4, cert.metadata_encryption_key());
  EXPECT_EQ(kEncryptedMetadataBytes4, cert.encrypted_metadata_bytes());
  EXPECT_EQ(kMetadataEncryptionKeyTag4, cert.metadata_encryption_key_tag());
}

TEST_F(NearbyShareCertificateStorageImplTest, AddPublicCertificates) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  std::vector<nearbyshare::proto::PublicCertificate> new_certs = {
      CreatePublicCertificate(kSecretId3, kSecretKey2, kPublicKey2,
                              kStartSeconds2, kStartNanos2, kEndSeconds2,
                              kEndNanos2, kForSelectedContacts2,
                              kMetadataEncryptionKey2, kEncryptedMetadataBytes2,
                              kMetadataEncryptionKeyTag2),
      CreatePublicCertificate(kSecretId4, kSecretKey4, kPublicKey4,
                              kStartSeconds4, kStartNanos4, kEndSeconds4,
                              kEndNanos4, kForSelectedContacts4,
                              kMetadataEncryptionKey4, kEncryptedMetadataBytes4,
                              kMetadataEncryptionKeyTag4),
  };

  bool succeeded = false;
  cert_store_->AddPublicCertificates(
      new_certs,
      base::BindOnce(
          &NearbyShareCertificateStorageImplTest::CaptureBoolCallback,
          base::Unretained(this), &succeeded));
  db_->UpdateCallback(true);

  ASSERT_TRUE(succeeded);
  ASSERT_EQ(4u, db_entries_.size());
  ASSERT_EQ(1u, db_entries_.count(kSecretId3));
  ASSERT_EQ(1u, db_entries_.count(kSecretId4));
  auto& cert = db_entries_.find(kSecretId3)->second;
  EXPECT_EQ(kSecretKey2, cert.secret_key());
  EXPECT_EQ(kPublicKey2, cert.public_key());
  EXPECT_EQ(kStartSeconds2, cert.start_time().seconds());
  EXPECT_EQ(kStartNanos2, cert.start_time().nanos());
  EXPECT_EQ(kEndSeconds2, cert.end_time().seconds());
  EXPECT_EQ(kEndNanos2, cert.end_time().nanos());
  EXPECT_EQ(kForSelectedContacts2, cert.for_selected_contacts());
  EXPECT_EQ(kMetadataEncryptionKey2, cert.metadata_encryption_key());
  EXPECT_EQ(kEncryptedMetadataBytes2, cert.encrypted_metadata_bytes());
  EXPECT_EQ(kMetadataEncryptionKeyTag2, cert.metadata_encryption_key_tag());
  cert = db_entries_.find(kSecretId4)->second;
  EXPECT_EQ(kSecretKey4, cert.secret_key());
  EXPECT_EQ(kPublicKey4, cert.public_key());
  EXPECT_EQ(kStartSeconds4, cert.start_time().seconds());
  EXPECT_EQ(kStartNanos4, cert.start_time().nanos());
  EXPECT_EQ(kEndSeconds4, cert.end_time().seconds());
  EXPECT_EQ(kEndNanos4, cert.end_time().nanos());
  EXPECT_EQ(kForSelectedContacts4, cert.for_selected_contacts());
  EXPECT_EQ(kMetadataEncryptionKey4, cert.metadata_encryption_key());
  EXPECT_EQ(kEncryptedMetadataBytes4, cert.encrypted_metadata_bytes());
  EXPECT_EQ(kMetadataEncryptionKeyTag4, cert.metadata_encryption_key_tag());
}

TEST_F(NearbyShareCertificateStorageImplTest, ClearPublicCertificates) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  bool succeeded = false;
  cert_store_->ClearPublicCertificates(base::BindOnce(
      &NearbyShareCertificateStorageImplTest::CaptureBoolCallback,
      base::Unretained(this), &succeeded));
  db_->DestroyCallback(true);

  ASSERT_TRUE(succeeded);
  ASSERT_EQ(0u, db_entries_.size());
}

TEST_F(NearbyShareCertificateStorageImplTest, RemoveExpiredPublicCertificates) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  std::vector<base::Time> expiration_times;
  for (const auto& pair : db_entries_) {
    expiration_times.emplace_back(TimestampToTime(pair.second.end_time()));
  }
  std::sort(expiration_times.begin(), expiration_times.end());
  base::Time now = expiration_times[1];

  bool succeeded = false;
  cert_store_->RemoveExpiredPublicCertificates(
      now, base::BindOnce(
               &NearbyShareCertificateStorageImplTest::CaptureBoolCallback,
               base::Unretained(this), &succeeded));
  db_->UpdateCallback(true);

  ASSERT_TRUE(succeeded);
  ASSERT_EQ(1u, db_entries_.size());
  for (const auto& pair : db_entries_) {
    EXPECT_LE(now, TimestampToTime(pair.second.end_time()));
  }
}

TEST_F(NearbyShareCertificateStorageImplTest, ReplaceGetPrivateCertificates) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  auto certs_before = CreatePrivateCertificates(3);
  cert_store_->ReplacePrivateCertificates(certs_before);
  auto certs_after = cert_store_->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  ASSERT_EQ(certs_before.size(), certs_after->size());
  for (size_t i = 0; i < certs_before.size(); ++i) {
    EXPECT_EQ(certs_before[i].ToDictionary(), (*certs_after)[i].ToDictionary());
  }

  certs_before = CreatePrivateCertificates(1);
  cert_store_->ReplacePrivateCertificates(certs_before);
  certs_after = cert_store_->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  ASSERT_EQ(certs_before.size(), certs_after->size());
  for (size_t i = 0; i < certs_before.size(); ++i) {
    EXPECT_EQ(certs_before[i].ToDictionary(), (*certs_after)[i].ToDictionary());
  }
}

TEST_F(NearbyShareCertificateStorageImplTest,
       NextPrivateCertificateExpirationTime) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  auto certs = CreatePrivateCertificates(3);
  cert_store_->ReplacePrivateCertificates(certs);
  base::Optional<base::Time> next_expiration =
      cert_store_->NextPrivateCertificateExpirationTime();

  ASSERT_TRUE(next_expiration.has_value());
  bool found = false;
  for (auto& cert : certs) {
    EXPECT_GE(cert.not_after(), *next_expiration);
    if (cert.not_after() == *next_expiration)
      found = true;
  }
  EXPECT_TRUE(found);
}

TEST_F(NearbyShareCertificateStorageImplTest,
       NextPublicCertificateExpirationTime) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  base::Optional<base::Time> next_expiration =
      cert_store_->NextPublicCertificateExpirationTime();

  ASSERT_TRUE(next_expiration.has_value());
  bool found = false;
  for (const auto& pair : db_entries_) {
    base::Time curr_expiration = TimestampToTime(pair.second.end_time());
    EXPECT_GE(curr_expiration, *next_expiration);
    if (curr_expiration == *next_expiration)
      found = true;
  }
  EXPECT_TRUE(found);
}

TEST_F(NearbyShareCertificateStorageImplTest, ClearPrivateCertificates) {
  Initialize(leveldb_proto::Enums::InitStatus::kOK);
  ASSERT_TRUE(cert_store_->IsInitialized());

  std::vector<NearbySharePrivateCertificate> certs_before =
      CreatePrivateCertificates(3);
  cert_store_->ReplacePrivateCertificates(certs_before);
  cert_store_->ClearPrivateCertificates();
  auto certs_after = cert_store_->GetPrivateCertificates();

  ASSERT_TRUE(certs_after.has_value());
  EXPECT_EQ(0u, certs_after->size());
}
