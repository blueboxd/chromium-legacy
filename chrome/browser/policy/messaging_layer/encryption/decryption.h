// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_DECRYPTION_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_DECRYPTION_H_

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {

// Interface to the encryption.
// Instantiated by an implementation-specific factory:
//   StatusOr<scoped_refptr<DecryptorBase>> Create(
//       implementation-specific parameters);
// The implementation class should never be used directly by the server code.
// Note: Production implementation should be written or enclosed in Java code
// for the server to use.
class DecryptorBase : public base::RefCountedThreadSafe<DecryptorBase> {
 public:
  // Decryption record handle, which is created by |OpenRecord| and can accept
  // pieces of data to be decrypted as one record by calling |AddToRecord|
  // multiple times. Resulting decrypted record is available once |CloseRecord|
  // is called.
  class Handle {
   public:
    // Adds piece of data to the record.
    virtual void AddToRecord(base::StringPiece data,
                             base::OnceCallback<void(Status)> cb) = 0;

    // Closes and attempts to decrypt the record. Hands over the decrypted data
    // to be processed by the server (or Status if unsuccessful). Accesses key
    // store to attempt all private keys that are considered to be valid,
    // starting with the one that matches the hash. Self-destructs after the
    // callback.
    virtual void CloseRecord(
        base::OnceCallback<void(StatusOr<base::StringPiece>)> cb) = 0;

   protected:
    explicit Handle(scoped_refptr<DecryptorBase> decryptor);

    // Destructor is non-public, because the object can only self-destruct by
    // |CloseRecord|.
    virtual ~Handle();

    DecryptorBase* decryptor() const { return decryptor_.get(); }

   private:
    scoped_refptr<DecryptorBase> decryptor_;
  };

  // Factory method creates new record to collect data and decrypt them with the
  // given encrypted key. Hands the handle raw pointer over to the callback, or
  // error status (e.g., “decryption is not enabled yet”)
  virtual void OpenRecord(base::StringPiece encrypted_key,
                          base::OnceCallback<void(StatusOr<Handle*>)> cb) = 0;

  // Decrypts symmetric key with asymmetric private key and returns unencrypted
  // key or error status (e.g., “decryption is not enabled yet”)
  virtual StatusOr<std::string> DecryptKey(base::StringPiece public_key,
                                           base::StringPiece encrypted_key) = 0;

  // Records a key pair (store only private key).
  // Executes on a sequenced thread, returns with callback.
  void RecordKeyPair(base::StringPiece private_key,
                     base::StringPiece public_key,
                     base::OnceCallback<void(Status)> cb);

  // Retrieves private key matching the public key hash.
  // Executes on a sequenced thread, returns with callback.
  void RetrieveMatchingPrivateKey(
      uint32_t public_key_id,
      base::OnceCallback<void(StatusOr<std::string>)> cb);

 protected:
  DecryptorBase();
  virtual ~DecryptorBase();

 private:
  friend base::RefCountedThreadSafe<DecryptorBase>;

  // Map of hash(public_key)->{public key, private key, time stamp}
  // Private key is located by the hash of a public key, sent together with the
  // encrypted record. Keys older than pre-defined threshold are discarded.
  struct KeyInfo {
    std::string private_key;
    base::Time time_stamp;
  };
  base::flat_map<uint32_t, KeyInfo> keys_;

  // Sequential task runner for all keys_ activities: recording, lookup, purge.
  scoped_refptr<base::SequencedTaskRunner> keys_sequenced_task_runner_;

  SEQUENCE_CHECKER(keys_sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_DECRYPTION_H_
