// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_IMPL_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_IMPL_H_

#include "base/macros.h"
#include "chromeos/components/multidevice/secure_message_delegate.h"

namespace chromeos {

class EasyUnlockClient;

namespace multidevice {

// Concrete SecureMessageDelegate implementation.
class SecureMessageDelegateImpl : public SecureMessageDelegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<SecureMessageDelegate> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<SecureMessageDelegate> CreateInstance() = 0;

   private:
    static Factory* test_factory_instance_;
  };

  ~SecureMessageDelegateImpl() override;

  // SecureMessageDelegate:
  void GenerateKeyPair(const GenerateKeyPairCallback& callback) override;
  void DeriveKey(const std::string& private_key,
                 const std::string& public_key,
                 const DeriveKeyCallback& callback) override;
  void CreateSecureMessage(
      const std::string& payload,
      const std::string& key,
      const CreateOptions& create_options,
      const CreateSecureMessageCallback& callback) override;
  void UnwrapSecureMessage(
      const std::string& serialized_message,
      const std::string& key,
      const UnwrapOptions& unwrap_options,
      const UnwrapSecureMessageCallback& callback) override;

 private:
  SecureMessageDelegateImpl();

  // Not owned by this instance.
  chromeos::EasyUnlockClient* dbus_client_;

  DISALLOW_COPY_AND_ASSIGN(SecureMessageDelegateImpl);
};

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_SECURE_MESSAGE_DELEGATE_IMPL_H_
