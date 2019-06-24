// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_support/grpc_async_server_streaming_request.h"

#include "base/bind.h"

namespace remoting {

namespace {

void RunTaskIfScopedStreamIsAlive(
    base::WeakPtr<ScopedGrpcServerStream> scoped_stream,
    base::OnceClosure task) {
  if (scoped_stream) {
    std::move(task).Run();
  }
}

}  // namespace

GrpcAsyncServerStreamingRequestBase::GrpcAsyncServerStreamingRequestBase(
    base::OnceClosure on_channel_ready,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
    std::unique_ptr<ScopedGrpcServerStream>* scoped_stream)
    : weak_factory_(this) {
  DCHECK(on_channel_closed);
  DCHECK_NE(nullptr, scoped_stream);
  on_channel_ready_ = std::move(on_channel_ready);
  on_channel_closed_ = std::move(on_channel_closed);
  *scoped_stream =
      std::make_unique<ScopedGrpcServerStream>(weak_factory_.GetWeakPtr());
  scoped_stream_ = (*scoped_stream)->GetWeakPtr();
}

GrpcAsyncServerStreamingRequestBase::~GrpcAsyncServerStreamingRequestBase() =
    default;

void GrpcAsyncServerStreamingRequestBase::RunTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(run_task_callback_);
  run_task_callback_.Run(base::BindOnce(&RunTaskIfScopedStreamIsAlive,
                                        scoped_stream_, std::move(task)));
}

bool GrpcAsyncServerStreamingRequestBase::OnDequeue(bool operation_succeeded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::CLOSED) {
    return false;
  }
  if (state_ == State::FINISHING) {
    DCHECK(operation_succeeded);
    state_ = State::CLOSED;
    ResolveChannelClosed();
    return false;
  }
  if (!operation_succeeded) {
    VLOG(1) << "Can't read any more data. Figuring out the reason..."
            << " Streaming call: " << this;
    state_ = State::FINISHING;
    return true;
  }
  if (state_ == State::STARTING) {
    VLOG(1) << "Streaming call started: " << this;
    state_ = State::PENDING_INITIAL_METADATA;
    return true;
  }
  if (state_ == State::PENDING_INITIAL_METADATA) {
    VLOG(1) << "Received initial metadata: " << this;
    state_ = State::STREAMING;
    ResolveChannelReady();
    return true;
  }
  if (state_ == State::STREAMING) {
    VLOG(1) << "Streaming call received message: " << this;
    ResolveIncomingMessage();
    return true;
  }
  NOTREACHED();
  return false;
}

void GrpcAsyncServerStreamingRequestBase::Reenqueue(void* event_tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::PENDING_INITIAL_METADATA:
      ReadInitialMetadata(event_tag);
      break;
    case State::STREAMING:
      WaitForIncomingMessage(event_tag);
      break;
    case State::FINISHING:
      FinishStream(event_tag);
      break;
    default:
      NOTREACHED() << "Unexpected state: " << static_cast<int>(state_);
      break;
  }
}

void GrpcAsyncServerStreamingRequestBase::OnRequestCanceled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::CLOSED;
  status_ = grpc::Status::CANCELLED;
  weak_factory_.InvalidateWeakPtrs();
}

bool GrpcAsyncServerStreamingRequestBase::CanStartRequest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_ == State::STARTING;
}

void GrpcAsyncServerStreamingRequestBase::ResolveChannelReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunTask(std::move(on_channel_ready_));
}

void GrpcAsyncServerStreamingRequestBase::ResolveChannelClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunTask(base::BindOnce(std::move(on_channel_closed_), status_));
}

}  // namespace remoting
