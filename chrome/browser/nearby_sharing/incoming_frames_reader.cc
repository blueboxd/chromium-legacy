// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"

#include <type_traits>

#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_connection.h"
#include "chrome/browser/nearby_sharing/nearby_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/sharing/public/mojom/nearby_decoder.mojom.h"

namespace {

std::ostream& operator<<(std::ostream& out,
                         const sharing::mojom::V1Frame::Tag& obj) {
  out << static_cast<std::underlying_type<sharing::mojom::V1Frame::Tag>::type>(
      obj);
  return out;
}

}  // namespace

IncomingFramesReader::IncomingFramesReader(
    NearbyProcessManager* process_manager,
    Profile* profile,
    NearbyConnection* connection)
    : process_manager_(process_manager),
      profile_(profile),
      connection_(connection) {
  DCHECK(process_manager_);
  DCHECK(profile_);
  DCHECK(connection_);

  nearby_process_observer_.Add(process_manager);
  connection->RegisterForDisconnection(
      base::BindOnce(&IncomingFramesReader::OnConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr()));
}

IncomingFramesReader::~IncomingFramesReader() = default;

void IncomingFramesReader::ReadFrame(
    sharing::mojom::V1Frame::Tag frame_type,
    base::OnceCallback<void(base::Optional<sharing::mojom::V1FramePtr>)>
        callback,
    base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_);
  DCHECK(!is_process_stopped_);
  if (!connection_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  callback_ = std::move(callback);
  frame_type_ = frame_type;

  timeout_callback_.Reset(base::BindOnce(&IncomingFramesReader::OnTimeout,
                                         weak_ptr_factory_.GetWeakPtr()));
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(timeout_callback_.callback()), timeout);

  // Check in cache for frame of type |frame_type|.
  auto iter = cached_frames_.find(frame_type);
  if (iter != cached_frames_.end()) {
    NS_LOG(VERBOSE) << __func__ << ": Successfully read cached frame of type "
                    << frame_type;
    sharing::mojom::V1FramePtr frame = std::move(iter->second);
    cached_frames_.erase(iter);
    Done(std::move(frame));
    return;
  }

  ReadNextFrame();
}

void IncomingFramesReader::OnNearbyProfileChanged(Profile* profile) {}

void IncomingFramesReader::OnNearbyProcessStarted() {}

void IncomingFramesReader::OnNearbyProcessStopped() {
  is_process_stopped_ = true;
  Done(base::nullopt);
}

void IncomingFramesReader::ReadNextFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_->Read(
      base::BindOnce(&IncomingFramesReader::OnDataReadFromConnection,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IncomingFramesReader::OnTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!callback_)
    return;

  NS_LOG(WARNING) << __func__ << ": Timed out reading from NearbyConnection.";
  connection_->Close();
}

void IncomingFramesReader::OnDataReadFromConnection(
    base::Optional<std::vector<uint8_t>> bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!callback_) {
    return;
  }

  if (!bytes) {
    NS_LOG(WARNING) << __func__ << ": Failed to read frame of type "
                    << frame_type_;
    Done(base::nullopt);
    return;
  }

  process_manager_->GetOrStartNearbySharingDecoder(profile_)->DecodeFrame(
      *bytes, base::BindOnce(&IncomingFramesReader::OnFrameDecoded,
                             weak_ptr_factory_.GetWeakPtr()));
}

void IncomingFramesReader::OnFrameDecoded(sharing::mojom::FramePtr frame) {
  if (!frame) {
    ReadNextFrame();
    return;
  }

  if (!frame->is_v1()) {
    NS_LOG(VERBOSE) << __func__ << ": Frame read does not have V1Frame";
    ReadNextFrame();
    return;
  }

  sharing::mojom::V1FramePtr v1_frame(std::move(frame->get_v1()));
  sharing::mojom::V1Frame::Tag v1_frame_type = v1_frame->which();
  if (frame_type_ != v1_frame_type) {
    NS_LOG(WARNING) << __func__ << ": Failed to read frame of type "
                    << frame_type_ << ", but got frame of type "
                    << v1_frame_type << ". Cached for later.";
    cached_frames_.insert({v1_frame_type, std::move(v1_frame)});
    ReadNextFrame();
    return;
  }

  NS_LOG(VERBOSE) << __func__ << ": Successfully read frame of type "
                  << v1_frame_type;
  Done(std::move(v1_frame));
}

void IncomingFramesReader::Done(
    base::Optional<sharing::mojom::V1FramePtr> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timeout_callback_.Cancel();
  if (callback_) {
    std::move(callback_).Run(std::move(frame));
  }
}

void IncomingFramesReader::OnConnectionClosed() {
  connection_ = nullptr;
}
