// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_GRPC_TEST_SUPPORT_FAKE_CLIENT_ASYNC_RESPONSE_READER_H_
#define REMOTING_BASE_GRPC_TEST_SUPPORT_FAKE_CLIENT_ASYNC_RESPONSE_READER_H_

#include <utility>

#include "base/callback.h"
#include "third_party/grpc/src/include/grpc/support/time.h"
#include "third_party/grpc/src/include/grpcpp/alarm.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"
#include "third_party/grpc/src/include/grpcpp/support/async_unary_call.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"

namespace remoting {
namespace test {

// Converts asynchronous stub calls to synchronous stub calls. Useful when
// creating mock StubInterface implementations: only the synchronous ops need to
// be mocked, while the async ops can return an instance of this class.
template <typename Response>
class FakeClientAsyncResponseReader
    : public grpc::ClientAsyncResponseReaderInterface<Response> {
 public:
  using SynchronousOp = base::OnceCallback<grpc::Status(Response* response)>;
  FakeClientAsyncResponseReader(SynchronousOp synchronous_op,
                                grpc::CompletionQueue* completion_queue,
                                bool start)
      : synchronous_op_(std::move(synchronous_op)),
        completion_queue_(completion_queue),
        started_(start) {}

  ~FakeClientAsyncResponseReader() override = default;

  void StartCall() override {
    DCHECK(!started_);
    started_ = true;
  }

  void ReadInitialMetadata(void* tag) override {
    alarm_.Set(completion_queue_, gpr_now(GPR_CLOCK_MONOTONIC), tag);
  }

  void Finish(Response* msg, grpc::Status* status, void* tag) override {
    DCHECK(started_);
    *status = std::move(synchronous_op_).Run(msg);
    alarm_.Set(completion_queue_, gpr_now(GPR_CLOCK_MONOTONIC), tag);
  }

 private:
  SynchronousOp synchronous_op_;
  grpc::CompletionQueue* completion_queue_;
  grpc::Alarm alarm_;
  bool started_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_BASE_GRPC_TEST_SUPPORT_FAKE_CLIENT_ASYNC_RESPONSE_READER_H_
