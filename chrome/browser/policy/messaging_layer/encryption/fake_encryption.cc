// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/fake_encryption.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/hash/hash.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "crypto/random.h"

namespace reporting {
namespace test {

namespace {
// Encryption record handle for FakeEncryptor.
class MockRecordHandle : public EncryptorBase::Handle {
 public:
  explicit MockRecordHandle(base::StringPiece symmetric_key,
                            scoped_refptr<EncryptorBase> encryptor)
      : Handle(encryptor), symmetric_key_(symmetric_key) {}

  MockRecordHandle(const MockRecordHandle& other) = delete;
  MockRecordHandle& operator=(const MockRecordHandle& other) = delete;

  void AddToRecord(base::StringPiece data,
                   base::OnceCallback<void(Status)> cb) override {
    // Append new data to the record.
    record_.append(data.data(), data.size());
    std::move(cb).Run(Status::StatusOK());
  }

  void CloseRecord(
      base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) override {
    // Encrypt all collected data in place by XORing every byte with the
    // symmetric key.
    size_t key_i = 0;
    for (auto& record_byte : record_) {
      record_byte ^= symmetric_key_[key_i++];
      if (key_i >= symmetric_key_.size()) {
        key_i = 0;
      }
    }
    // Encrypt the symmetric key.
    encryptor()->EncryptKey(
        symmetric_key_,
        base::BindOnce(
            [](MockRecordHandle* handle,
               base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb,
               StatusOr<std::pair<uint32_t, std::string>>
                   encrypted_key_result) {
              if (!encrypted_key_result.ok()) {
                std::move(cb).Run(encrypted_key_result.status());
              } else {
                EncryptedRecord encrypted_record;
                encrypted_record.mutable_encryption_info()->set_public_key_id(
                    encrypted_key_result.ValueOrDie().first);
                encrypted_record.mutable_encryption_info()->set_encryption_key(
                    encrypted_key_result.ValueOrDie().second);
                encrypted_record.set_encrypted_wrapped_record(handle->record_);
                std::move(cb).Run(encrypted_record);
              }
              delete handle;
            },
            base::Unretained(this),  // will self-destruct.
            std::move(cb)));
  }

 private:
  // Symmetric key.
  const std::string symmetric_key_;

  // Accumulated encrypted data.
  std::string record_;
};

}  // namespace

StatusOr<scoped_refptr<EncryptorBase>> FakeEncryptor::Create() {
  return base::WrapRefCounted(new FakeEncryptor());
}

FakeEncryptor::FakeEncryptor() = default;
FakeEncryptor::~FakeEncryptor() = default;

void FakeEncryptor::OpenRecord(base::OnceCallback<void(StatusOr<Handle*>)> cb) {
  // For fake implementation just generate random byte string.
  constexpr size_t symmetric_key_size = 8;
  char symmetric_key[8];
  crypto::RandBytes(symmetric_key, symmetric_key_size);
  std::move(cb).Run(new MockRecordHandle(
      std::string(symmetric_key, symmetric_key_size), this));
}

std::string FakeEncryptor::EncryptSymmetricKey(
    base::StringPiece symmetric_key,
    base::StringPiece asymmetric_key) {
  // Encrypt symmetric key with public asymmetric one: XOR byte by byte.
  std::string encrypted_key;
  encrypted_key.reserve(symmetric_key.size());
  size_t asymmetric_i = 0;
  for (const auto& key_byte : symmetric_key) {
    encrypted_key.push_back(key_byte ^ asymmetric_key[asymmetric_i++]);
    if (asymmetric_i >= asymmetric_key.size()) {
      asymmetric_i = 0;
    }
  }
  return encrypted_key;
}

}  // namespace test
}  // namespace reporting
