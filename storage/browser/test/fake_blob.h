// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_FAKE_BLOB_H_
#define STORAGE_BROWSER_TEST_FAKE_BLOB_H_

#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace storage {

// Implementation of the blink::mojom::Blob interface. Only supports the Clone
// and GetInternalUUID methods.
class FakeBlob : public blink::mojom::Blob {
 public:
  explicit FakeBlob(const std::string& uuid);

  blink::mojom::BlobPtr Clone();
  void Clone(blink::mojom::BlobRequest request) override;
  void AsDataPipeGetter(network::mojom::DataPipeGetterRequest) override;
  void ReadRange(uint64_t offset,
                 uint64_t size,
                 mojo::ScopedDataPipeProducerHandle,
                 mojo::PendingRemote<blink::mojom::BlobReaderClient>) override;
  void ReadAll(mojo::ScopedDataPipeProducerHandle,
               mojo::PendingRemote<blink::mojom::BlobReaderClient>) override;
  void ReadSideData(ReadSideDataCallback) override;
  void GetInternalUUID(GetInternalUUIDCallback callback) override;

 private:
  std::string uuid_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_FAKE_BLOB_H_
