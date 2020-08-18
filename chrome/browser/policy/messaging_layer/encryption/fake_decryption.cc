// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/fake_decryption.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {
namespace test {

namespace {
// Decryption record handle for FakeDecryptor.
class MockRecordHandle : public DecryptorBase::Handle {
 public:
  explicit MockRecordHandle(base::StringPiece symmetric_key,
                            scoped_refptr<DecryptorBase> decryptor)
      : Handle(decryptor), symmetric_key_(symmetric_key) {}

  MockRecordHandle(const MockRecordHandle& other) = delete;
  MockRecordHandle& operator=(const MockRecordHandle& other) = delete;

  void AddToRecord(base::StringPiece data,
                   base::OnceCallback<void(Status)> cb) override {
    // Add piece of data to the record.
    record_.append(data.data(), data.size());
    std::move(cb).Run(Status::StatusOK());
  }

  void CloseRecord(
      base::OnceCallback<void(StatusOr<base::StringPiece>)> cb) override {
    // Decrypt data in place by XORing every byte with the bytes of symmetric
    // key.
    size_t key_i = 0;
    for (auto& record_byte : record_) {
      record_byte ^= symmetric_key_[key_i++];
      if (key_i >= symmetric_key_.size()) {
        key_i = 0;
      }
    }
    std::move(cb).Run(record_);
    delete this;
  }

 private:
  // Symmetric key.
  const std::string symmetric_key_;

  // Accumulated decrypted data.
  std::string record_;
};

}  // namespace

StatusOr<scoped_refptr<DecryptorBase>> FakeDecryptor::Create() {
  return base::WrapRefCounted(new FakeDecryptor());
}

FakeDecryptor::FakeDecryptor() = default;
FakeDecryptor::~FakeDecryptor() = default;

void FakeDecryptor::OpenRecord(base::StringPiece encrypted_key,
                               base::OnceCallback<void(StatusOr<Handle*>)> cb) {
  std::move(cb).Run(new MockRecordHandle(encrypted_key, this));
}

StatusOr<std::string> FakeDecryptor::DecryptKey(
    base::StringPiece private_key,
    base::StringPiece encrypted_key) {
  if (private_key.empty()) {
    return Status{error::FAILED_PRECONDITION, "Private key not provided"};
  }
  // Decrypt symmetric key.
  // Private key is assumed to be a reverse string to the public key.
  // If symmetric key was encrypted XORing bytes with a public key "012",
  // decryption will use private key "210" and XOR will be from the last to the
  // first bytes.
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

}  // namespace test
}  // namespace reporting
