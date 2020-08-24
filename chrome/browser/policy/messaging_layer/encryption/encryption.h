// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"

namespace reporting {

// Interface to the encryption.
// Instantiated by an implementation-specific factory:
//   StatusOr<scoped_refptr<EncryptorBase>> Create(
//       implementation-specific parameters);
// The implementation class should never be used directly by the client code.
class EncryptorBase : public base::RefCountedThreadSafe<EncryptorBase> {
 public:
  // Encryption record handle, which is created by |OpenRecord| and can accept
  // pieces of data to be encrypted as one record by calling |AddToRecord|
  // multiple times. Resulting encrypted record is available once |CloseRecord|
  // is called.
  class Handle {
   public:
    // Adds piece of data to the record.
    virtual void AddToRecord(base::StringPiece data,
                             base::OnceCallback<void(Status)> cb) = 0;

    // Closes and encrypts the record, hands over the data (encrypted with
    // symmetric key) and the key (encrypted with asymmetric key) to be recorded
    // by the client (or Status if unsuccessful). Self-destructs after the
    // callback.
    virtual void CloseRecord(
        base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) = 0;

   protected:
    explicit Handle(scoped_refptr<EncryptorBase> encryptor);

    // Destructor is non-public, because the object can only self-destruct by
    // |CloseRecord|.
    virtual ~Handle();

    EncryptorBase* encryptor() const { return encryptor_.get(); }

   private:
    scoped_refptr<EncryptorBase> encryptor_;
  };

  // Delivers public asymmetric key to the implementation.
  // To affect specific record, must happen before Handle::CloseRecord
  // (it is OK to do it after OpenRecord and Handle::AddToRecord).
  // Executes on a sequenced thread, returns with callback.
  void UpdateAsymmetricKey(base::StringPiece new_key,
                           base::OnceCallback<void(Status)> response_cb);

  // Factory method creates new record to collect data and encrypt them.
  // Hands the Handle raw pointer over to the callback, or error status
  // (e.g., “encryption is not enabled yet”).
  virtual void OpenRecord(base::OnceCallback<void(StatusOr<Handle*>)> cb) = 0;

  // Encrypts symmetric key with asymmetric public key, returns encrypted key
  // and the hash of the public key used or error status (e.g., “decryption is
  // not enabled yet”)
  void EncryptKey(
      base::StringPiece key,
      base::OnceCallback<void(StatusOr<std::pair<uint32_t, std::string>>)> cb);

 protected:
  EncryptorBase();
  virtual ~EncryptorBase();

 private:
  friend class base::RefCountedThreadSafe<EncryptorBase>;

  // Synchronously encrypts symmetric key with asymmetric.
  // Called by |EncryptKey|.
  virtual std::string EncryptSymmetricKey(base::StringPiece symmetric_key,
                                          base::StringPiece asymmetric_key) = 0;

  // Retrieves the current public key.
  // Executes on a sequenced thread, returns with callback.
  void RetrieveAsymmetricKey(
      base::OnceCallback<void(StatusOr<std::string>)> cb);

  // Public key used for asymmetric encryption of symmetric key.
  base::Optional<std::string> asymmetric_key_;

  // Sequential task runner for all asymmetric_key_ activities: update, read.
  scoped_refptr<base::SequencedTaskRunner>
      asymmetric_key_sequenced_task_runner_;

  SEQUENCE_CHECKER(asymmetric_key_sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_ENCRYPTION_H_
