// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_MODULE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_MODULE_H_

#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

// TODO(b/153659559) Temporary StorageModule until the real one is ready.
class StorageModule : public base::RefCounted<StorageModule> {
 public:
  StorageModule() = default;

  StorageModule(const StorageModule& other) = delete;
  StorageModule& operator=(const StorageModule& other) = delete;

  // AddRecord will add |record| to the |StorageModule| according to the
  // provided |priority|. On completion, |callback| will be called.
  virtual void AddRecord(reporting::EncryptedRecord record,
                         reporting::Priority priority,
                         base::OnceCallback<void(Status)> callback);

 protected:
  virtual ~StorageModule() = default;

 private:
  friend base::RefCounted<StorageModule>;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_MODULE_H_
