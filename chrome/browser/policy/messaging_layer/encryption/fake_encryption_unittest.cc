// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/fake_encryption.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/policy/messaging_layer/encryption/fake_decryption.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//       base::OnceCallback<void(ResType* res)> type which also may perform
//       some other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//                      // collected result.
//
// Or, when the callback is not expected to be invoked:
//
//   TestEvent<ResType> e(/*expected_to_complete=*/false);
//   ... Start work passing e.cb() as a completion callback,
//       which will not happen.
//
template <typename ResType>
class TestEvent {
 public:
  explicit TestEvent(bool expected_to_complete = true)
      : expected_to_complete_(expected_to_complete),
        completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~TestEvent() {
    if (expected_to_complete_) {
      EXPECT_TRUE(completed_.IsSignaled()) << "Not responded";
    } else {
      EXPECT_FALSE(completed_.IsSignaled()) << "Responded";
    }
  }
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    completed_.Wait();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    DCHECK(!completed_.IsSignaled());
    return base::BindOnce(
        [](base::WaitableEvent* completed, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          completed->Signal();
        },
        base::Unretained(&completed_), base::Unretained(&result_));
  }

 private:
  bool expected_to_complete_;
  base::WaitableEvent completed_;
  ResType result_;
};

class FakeEncryptionTest : public ::testing::Test {
 protected:
  FakeEncryptionTest() = default;

  void SetUp() override {
    auto encryptor_result = test::FakeEncryptor::Create();
    ASSERT_OK(encryptor_result.status()) << encryptor_result.status();
    encryptor_ = std::move(encryptor_result.ValueOrDie());

    auto decryptor_result = test::FakeDecryptor::Create();
    ASSERT_OK(decryptor_result.status()) << decryptor_result.status();
    decryptor_ = std::move(decryptor_result.ValueOrDie());
  }

  StatusOr<EncryptedRecord> EncryptSync(base::StringPiece data) {
    TestEvent<StatusOr<EncryptorBase::Handle*>> open_encrypt;
    encryptor_->OpenRecord(open_encrypt.cb());
    auto open_encrypt_result = open_encrypt.result();
    RETURN_IF_ERROR(open_encrypt_result.status());
    EncryptorBase::Handle* const enc_handle = open_encrypt_result.ValueOrDie();

    TestEvent<Status> add_encrypt;
    enc_handle->AddToRecord(data, add_encrypt.cb());
    RETURN_IF_ERROR(add_encrypt.result());

    EncryptedRecord encrypted;
    TestEvent<Status> close_encrypt;
    enc_handle->CloseRecord(base::BindOnce(
        [](EncryptedRecord* encrypted,
           base::OnceCallback<void(Status)> close_cb,
           StatusOr<EncryptedRecord> result) {
          if (!result.ok()) {
            std::move(close_cb).Run(result.status());
            return;
          }
          *encrypted = result.ValueOrDie();
          std::move(close_cb).Run(Status::StatusOK());
        },
        base::Unretained(&encrypted), close_encrypt.cb()));
    RETURN_IF_ERROR(close_encrypt.result());
    return encrypted;
  }

  StatusOr<std::string> DecryptSync(
      std::pair<std::string /*unencrypted_key*/, std::string /*encrypted_data*/>
          encrypted) {
    TestEvent<StatusOr<DecryptorBase::Handle*>> open_decrypt;
    decryptor_->OpenRecord(encrypted.first, open_decrypt.cb());
    auto open_decrypt_result = open_decrypt.result();
    RETURN_IF_ERROR(open_decrypt_result.status());
    DecryptorBase::Handle* const dec_handle = open_decrypt_result.ValueOrDie();

    TestEvent<Status> add_decrypt;
    dec_handle->AddToRecord(encrypted.second, add_decrypt.cb());
    RETURN_IF_ERROR(add_decrypt.result());

    std::string decrypted_string;
    TestEvent<Status> close_decrypt;
    dec_handle->CloseRecord(base::BindOnce(
        [](std::string* decrypted_string,
           base::OnceCallback<void(Status)> close_cb,
           StatusOr<base::StringPiece> result) {
          if (!result.ok()) {
            std::move(close_cb).Run(result.status());
            return;
          }
          *decrypted_string = std::string(result.ValueOrDie());
          std::move(close_cb).Run(Status::StatusOK());
        },
        base::Unretained(&decrypted_string), close_decrypt.cb()));
    RETURN_IF_ERROR(close_decrypt.result());
    return decrypted_string;
  }

  StatusOr<std::string> DecryptMatchingKey(uint32_t public_key_id,
                                           base::StringPiece encrypted_key) {
    // Retrieve private key that matches public key hash.
    TestEvent<StatusOr<std::string>> retrieve_private_key;
    decryptor_->RetrieveMatchingPrivateKey(public_key_id,
                                           retrieve_private_key.cb());
    ASSIGN_OR_RETURN(std::string private_key, retrieve_private_key.result());
    // Decrypt symmetric key with that private key.
    std::string unencrypted_key;
    unencrypted_key.reserve(encrypted_key.size());
    size_t key_i = 0;
    for (const auto& key_byte : encrypted_key) {
      unencrypted_key.push_back(key_byte ^
                                private_key[private_key.size() - ++key_i]);
      if (key_i >= private_key.size()) {
        key_i = 0;
      }
    }
    return unencrypted_key;
  }

  scoped_refptr<EncryptorBase> encryptor_;
  scoped_refptr<DecryptorBase> decryptor_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(FakeEncryptionTest, EncryptAndDecrypt) {
  constexpr char kTestString[] = "ABCDEF";
  // Public and private key in this test are reversed strings.
  constexpr char kPublicKeyString[] = "123";
  constexpr char kPrivateKeyString[] = "321";

  // Register key pair and provide public key to the encryptor.
  TestEvent<Status> record_keys;
  decryptor_->RecordKeyPair(kPrivateKeyString, kPublicKeyString,
                            record_keys.cb());
  ASSERT_OK(record_keys.result()) << record_keys.result();
  TestEvent<Status> set_public_key;
  encryptor_->UpdateAsymmetricKey(kPublicKeyString, set_public_key.cb());
  ASSERT_OK(set_public_key.result());

  // Encrypt the test string.
  const auto encrypted_result = EncryptSync(kTestString);
  ASSERT_OK(encrypted_result.status()) << encrypted_result.status();

  // Decrypt encrypted_key with private asymmetric key.
  auto decrypt_key_result = DecryptMatchingKey(
      encrypted_result.ValueOrDie().encryption_info().public_key_id(),
      encrypted_result.ValueOrDie().encryption_info().encryption_key());
  ASSERT_OK(decrypt_key_result.status()) << decrypt_key_result.status();

  // Decrypt back.
  const auto decrypted_result = DecryptSync(
      std::make_pair(decrypt_key_result.ValueOrDie(),
                     encrypted_result.ValueOrDie().encrypted_wrapped_record()));
  ASSERT_OK(decrypted_result.status()) << decrypted_result.status();

  EXPECT_THAT(decrypted_result.ValueOrDie(), ::testing::StrEq(kTestString));
}

TEST_F(FakeEncryptionTest, NoPublicKey) {
  constexpr char kTestString[] = "ABCDEF";

  // Attempt to encrypt the test string.
  const auto encrypted_result = EncryptSync(kTestString);
  EXPECT_EQ(encrypted_result.status().error_code(), error::NOT_FOUND);
}

TEST_F(FakeEncryptionTest, EncryptAndDecryptMultiple) {
  constexpr const char* kTestStrings[] = {"Rec1",    "Rec22",    "Rec333",
                                          "Rec4444", "Rec55555", "Rec666666"};
  // Public and private key pairs in this test are reversed strings.
  constexpr const char* kPublicKeyStrings[] = {"123", "45", "7"};
  constexpr const char* kPrivateKeyStrings[] = {"321", "54", "7"};
  // Encrypted records.
  std::vector<EncryptedRecord> encrypted_records;

  // 1. Register first key pair.
  TestEvent<Status> record_keys_0;
  decryptor_->RecordKeyPair(kPrivateKeyStrings[0], kPublicKeyStrings[0],
                            record_keys_0.cb());
  ASSERT_OK(record_keys_0.result()) << record_keys_0.result();
  TestEvent<Status> set_public_key_0;
  encryptor_->UpdateAsymmetricKey(kPublicKeyStrings[0], set_public_key_0.cb());
  ASSERT_OK(set_public_key_0.result()) << set_public_key_0.result();

  // 2. Encrypt 3 test strings.
  for (const char* test_string :
       {kTestStrings[0], kTestStrings[1], kTestStrings[2]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_OK(encrypted_result.status()) << encrypted_result.status();
    encrypted_records.emplace_back(encrypted_result.ValueOrDie());
  }

  // 3. Register second key pair.
  TestEvent<Status> record_keys_1;
  decryptor_->RecordKeyPair(kPrivateKeyStrings[1], kPublicKeyStrings[1],
                            record_keys_1.cb());
  ASSERT_OK(record_keys_1.result()) << record_keys_1.result();
  TestEvent<Status> set_public_key_1;
  encryptor_->UpdateAsymmetricKey(kPublicKeyStrings[1], set_public_key_1.cb());
  ASSERT_OK(set_public_key_1.result()) << set_public_key_1.result();

  // 4. Encrypt 2 test strings.
  for (const char* test_string : {kTestStrings[3], kTestStrings[4]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_OK(encrypted_result.status()) << encrypted_result.status();
    encrypted_records.emplace_back(encrypted_result.ValueOrDie());
  }

  // 3. Register third key pair.
  TestEvent<Status> record_keys_2;
  decryptor_->RecordKeyPair(kPrivateKeyStrings[2], kPublicKeyStrings[2],
                            record_keys_2.cb());
  ASSERT_OK(record_keys_2.result()) << record_keys_2.result();
  TestEvent<Status> set_public_key_2;
  encryptor_->UpdateAsymmetricKey(kPublicKeyStrings[2], set_public_key_2.cb());
  ASSERT_OK(set_public_key_2.result()) << set_public_key_2.result();

  // 4. Encrypt one more test strings.
  for (const char* test_string : {kTestStrings[5]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_OK(encrypted_result.status()) << encrypted_result.status();
    encrypted_records.emplace_back(encrypted_result.ValueOrDie());
  }

  // For every encrypted record:
  for (size_t i = 0; i < encrypted_records.size(); ++i) {
    // Decrypt encrypted_key with private asymmetric key.
    auto decrypt_key_result = DecryptMatchingKey(
        encrypted_records[i].encryption_info().public_key_id(),
        encrypted_records[i].encryption_info().encryption_key());
    ASSERT_OK(decrypt_key_result.status()) << decrypt_key_result.status();

    // Decrypt back.
    const auto decrypted_result = DecryptSync(
        std::make_pair(decrypt_key_result.ValueOrDie(),
                       encrypted_records[i].encrypted_wrapped_record()));
    ASSERT_OK(decrypted_result.status()) << decrypted_result.status();

    // Verify match.
    EXPECT_THAT(decrypted_result.ValueOrDie(),
                ::testing::StrEq(kTestStrings[i]));
  }
}

TEST_F(FakeEncryptionTest, EncryptAndDecryptMultipleParallel) {
  // Context of single encryption. Self-destructs upon completion or failure.
  class SingleEncryptionContext {
   public:
    SingleEncryptionContext(
        base::StringPiece test_string,
        base::StringPiece public_key,
        scoped_refptr<EncryptorBase> encryptor,
        base::OnceCallback<void(StatusOr<EncryptedRecord>)> response)
        : test_string_(test_string),
          public_key_(public_key),
          encryptor_(encryptor),
          response_(std::move(response)) {}

    SingleEncryptionContext(const SingleEncryptionContext& other) = delete;
    SingleEncryptionContext& operator=(const SingleEncryptionContext& other) =
        delete;

    ~SingleEncryptionContext() {
      DCHECK(!response_) << "Self-destruct without prior response";
    }

    void Start() {
      base::ThreadPool::PostTask(
          FROM_HERE, base::BindOnce(&SingleEncryptionContext::SetPublicKey,
                                    base::Unretained(this)));
    }

   private:
    void Respond(StatusOr<EncryptedRecord> result) {
      std::move(response_).Run(result);
      delete this;
    }
    void SetPublicKey() {
      encryptor_->UpdateAsymmetricKey(
          public_key_,
          base::BindOnce(
              [](SingleEncryptionContext* self, Status status) {
                if (!status.ok()) {
                  self->Respond(status);
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleEncryptionContext::OpenRecord,
                                   base::Unretained(self)));
              },
              base::Unretained(this)));
    }
    void OpenRecord() {
      encryptor_->OpenRecord(base::BindOnce(
          [](SingleEncryptionContext* self,
             StatusOr<EncryptorBase::Handle*> handle_result) {
            if (!handle_result.ok()) {
              self->Respond(handle_result.status());
              return;
            }
            base::ThreadPool::PostTask(
                FROM_HERE,
                base::BindOnce(&SingleEncryptionContext::AddToRecord,
                               base::Unretained(self),
                               base::Unretained(handle_result.ValueOrDie())));
          },
          base::Unretained(this)));
    }
    void AddToRecord(EncryptorBase::Handle* handle) {
      handle->AddToRecord(
          test_string_,
          base::BindOnce(
              [](SingleEncryptionContext* self, EncryptorBase::Handle* handle,
                 Status status) {
                if (!status.ok()) {
                  self->Respond(status);
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleEncryptionContext::CloseRecord,
                                   base::Unretained(self),
                                   base::Unretained(handle)));
              },
              base::Unretained(this), base::Unretained(handle)));
    }
    void CloseRecord(EncryptorBase::Handle* handle) {
      handle->CloseRecord(base::BindOnce(
          [](SingleEncryptionContext* self,
             StatusOr<EncryptedRecord> encryption_result) {
            self->Respond(encryption_result);
          },
          base::Unretained(this)));
    }

   private:
    const std::string test_string_;
    const std::string public_key_;
    const scoped_refptr<EncryptorBase> encryptor_;
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> response_;
  };

  // Context of single decryption. Self-destructs upon completion or failure.
  class SingleDecryptionContext {
   public:
    SingleDecryptionContext(
        const EncryptedRecord& encrypted_record,
        scoped_refptr<DecryptorBase> decryptor,
        base::OnceCallback<void(StatusOr<base::StringPiece>)> response)
        : encrypted_record_(encrypted_record),
          decryptor_(decryptor),
          response_(std::move(response)) {}

    SingleDecryptionContext(const SingleDecryptionContext& other) = delete;
    SingleDecryptionContext& operator=(const SingleDecryptionContext& other) =
        delete;

    ~SingleDecryptionContext() {
      DCHECK(!response_) << "Self-destruct without prior response";
    }

    void Start() {
      base::ThreadPool::PostTask(
          FROM_HERE,
          base::BindOnce(&SingleDecryptionContext::RetrieveMatchingPrivateKey,
                         base::Unretained(this)));
    }

   private:
    void Respond(StatusOr<base::StringPiece> result) {
      std::move(response_).Run(result);
      delete this;
    }

    void RetrieveMatchingPrivateKey() {
      // Retrieve private key that matches public key hash.
      decryptor_->RetrieveMatchingPrivateKey(
          encrypted_record_.encryption_info().public_key_id(),
          base::BindOnce(
              [](SingleDecryptionContext* self,
                 StatusOr<std::string> private_key_result) {
                if (!private_key_result.ok()) {
                  self->Respond(private_key_result.status());
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        &SingleDecryptionContext::DecryptSymmetricKey,
                        base::Unretained(self),
                        private_key_result.ValueOrDie()));
              },
              base::Unretained(this)));
    }

    void DecryptSymmetricKey(base::StringPiece private_key) {
      // Decrypt symmetric key with that private key.
      std::string unencrypted_key;
      unencrypted_key.reserve(
          encrypted_record_.encryption_info().encryption_key().size());
      size_t key_i = 0;
      for (const auto& key_byte :
           encrypted_record_.encryption_info().encryption_key()) {
        unencrypted_key.push_back(key_byte ^
                                  private_key[private_key.size() - ++key_i]);
        if (key_i >= private_key.size()) {
          key_i = 0;
        }
      }
      base::ThreadPool::PostTask(
          FROM_HERE, base::BindOnce(&SingleDecryptionContext::OpenRecord,
                                    base::Unretained(this), unencrypted_key));
    }

    void OpenRecord(base::StringPiece unencrypted_key) {
      decryptor_->OpenRecord(
          unencrypted_key,
          base::BindOnce(
              [](SingleDecryptionContext* self,
                 StatusOr<DecryptorBase::Handle*> handle_result) {
                if (!handle_result.ok()) {
                  self->Respond(handle_result.status());
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        &SingleDecryptionContext::AddToRecord,
                        base::Unretained(self),
                        base::Unretained(handle_result.ValueOrDie())));
              },
              base::Unretained(this)));
    }

    void AddToRecord(DecryptorBase::Handle* handle) {
      handle->AddToRecord(
          encrypted_record_.encrypted_wrapped_record(),
          base::BindOnce(
              [](SingleDecryptionContext* self, DecryptorBase::Handle* handle,
                 Status status) {
                if (!status.ok()) {
                  self->Respond(status);
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleDecryptionContext::CloseRecord,
                                   base::Unretained(self),
                                   base::Unretained(handle)));
              },
              base::Unretained(this), base::Unretained(handle)));
    }

    void CloseRecord(DecryptorBase::Handle* handle) {
      handle->CloseRecord(base::BindOnce(
          [](SingleDecryptionContext* self,
             StatusOr<base::StringPiece> decryption_result) {
            self->Respond(decryption_result);
          },
          base::Unretained(this)));
    }

   private:
    const EncryptedRecord encrypted_record_;
    const scoped_refptr<DecryptorBase> decryptor_;
    base::OnceCallback<void(StatusOr<base::StringPiece>)> response_;
  };

  constexpr std::array<const char*, 6> kTestStrings = {
      "Rec1", "Rec22", "Rec333", "Rec4444", "Rec55555", "Rec666666"};
  // Public and private key pairs in this test are reversed strings.
  constexpr std::array<const char*, 3> kPublicKeyStrings = {"123", "45", "7"};
  constexpr std::array<const char*, 3> kPrivateKeyStrings = {"321", "54", "7"};

  // Encrypt all records in parallel.
  std::vector<TestEvent<StatusOr<EncryptedRecord>>> results(
      kTestStrings.size());
  for (size_t i = 0; i < kTestStrings.size(); ++i) {
    // Choose random key pair.
    size_t i_key_pair = base::RandInt(0, kPublicKeyStrings.size() - 1);
    (new SingleEncryptionContext(kTestStrings[i], kPublicKeyStrings[i_key_pair],
                                 encryptor_, results[i].cb()))
        ->Start();
  }

  // Register all key pairs for decryption.
  std::vector<TestEvent<Status>> record_results(kPublicKeyStrings.size());
  for (size_t i = 0; i < kPublicKeyStrings.size(); ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const char* public_key_string, const char* private_key_string,
               scoped_refptr<DecryptorBase> decryptor,
               base::OnceCallback<void(Status)> done_cb) {
              decryptor->RecordKeyPair(private_key_string, public_key_string,
                                       std::move(done_cb));
            },
            kPublicKeyStrings[i], kPrivateKeyStrings[i], decryptor_,
            record_results[i].cb()));
  }
  // Verify registration success.
  for (auto& record_result : record_results) {
    ASSERT_OK(record_result.result()) << record_result.result();
  }

  // Decrypt all records in parallel.
  std::vector<TestEvent<StatusOr<std::string>>> decryption_results(
      kTestStrings.size());
  for (size_t i = 0; i < results.size(); ++i) {
    // Verify encryption success.
    const auto result = results[i].result();
    ASSERT_OK(result.status()) << result.status();
    // Decrypt and compare encrypted_record.
    (new SingleDecryptionContext(
         result.ValueOrDie(), decryptor_,
         base::BindOnce(
             [](base::OnceCallback<void(StatusOr<std::string>)>
                    decryption_result,
                StatusOr<base::StringPiece> result) {
               if (!result.ok()) {
                 std::move(decryption_result).Run(result.status());
                 return;
               }
               std::move(decryption_result)
                   .Run(std::string(result.ValueOrDie()));
             },
             decryption_results[i].cb())))
        ->Start();
  }

  // Verify decryption results.
  for (size_t i = 0; i < decryption_results.size(); ++i) {
    const auto decryption_result = decryption_results[i].result();
    ASSERT_OK(decryption_result.status()) << decryption_result.status();
    // Verify data match.
    EXPECT_THAT(decryption_result.ValueOrDie(),
                ::testing::StrEq(kTestStrings[i]));
  }
}

}  // namespace
}  // namespace reporting
