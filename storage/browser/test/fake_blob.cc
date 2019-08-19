// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/fake_blob.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace storage {

FakeBlob::FakeBlob(const std::string& uuid) : uuid_(uuid) {}

blink::mojom::BlobPtr FakeBlob::Clone() {
  blink::mojom::BlobPtr result;
  Clone(MakeRequest(&result));
  return result;
}

void FakeBlob::Clone(blink::mojom::BlobRequest request) {
  mojo::MakeStrongBinding(std::make_unique<FakeBlob>(uuid_),
                          std::move(request));
}

void FakeBlob::AsDataPipeGetter(network::mojom::DataPipeGetterRequest) {
  NOTREACHED();
}
void FakeBlob::ReadRange(uint64_t offset,
                         uint64_t size,
                         mojo::ScopedDataPipeProducerHandle,
                         mojo::PendingRemote<blink::mojom::BlobReaderClient>) {
  NOTREACHED();
}

void FakeBlob::ReadAll(mojo::ScopedDataPipeProducerHandle,
                       mojo::PendingRemote<blink::mojom::BlobReaderClient>) {
  NOTREACHED();
}

void FakeBlob::ReadSideData(ReadSideDataCallback) {
  NOTREACHED();
}

void FakeBlob::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(uuid_);
}

}  // namespace storage
