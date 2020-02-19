// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/test_api/test_api.h"

#include "base/bind.h"
#include "base/immediate_crash.h"
#include "base/no_destructor.h"
#include "components/services/storage/public/mojom/test_api.test-mojom.h"
#include "components/services/storage/test_api_stubs.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace storage {

namespace {

class TestApiImpl : public mojom::TestApi {
 public:
  TestApiImpl() = default;
  ~TestApiImpl() override = default;

  void AddReceiver(mojo::PendingReceiver<mojom::TestApi> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  // mojom::TestApi implementation:
  void CrashNow() override { IMMEDIATE_CRASH(); }

 private:
  mojo::ReceiverSet<mojom::TestApi> receivers_;
};

void BindTestApi(mojo::ScopedMessagePipeHandle test_api_receiver) {
  static base::NoDestructor<TestApiImpl> impl;
  impl->AddReceiver(
      mojo::PendingReceiver<mojom::TestApi>(std::move(test_api_receiver)));
}

}  // namespace

void InjectTestApiImplementation() {
  SetTestApiBinderForTesting(base::BindRepeating(&BindTestApi));
}

}  // namespace storage
