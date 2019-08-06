// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/fuchsia_cdm.h"

#include "base/logging.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_promise.h"

namespace media {

FuchsiaCdm::SessionCallbacks::SessionCallbacks() = default;
FuchsiaCdm::SessionCallbacks::SessionCallbacks(SessionCallbacks&&) = default;
FuchsiaCdm::SessionCallbacks::~SessionCallbacks() = default;
FuchsiaCdm::SessionCallbacks& FuchsiaCdm::SessionCallbacks::operator=(
    SessionCallbacks&&) = default;

FuchsiaCdm::FuchsiaCdm(fuchsia::media::drm::ContentDecryptionModulePtr cdm,
                       SessionCallbacks callbacks)
    : cdm_(std::move(cdm)), session_callbacks_(std::move(callbacks)) {
  DCHECK(cdm_);
}

FuchsiaCdm::~FuchsiaCdm() = default;

void FuchsiaCdm::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  NOTIMPLEMENTED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "not implemented");
}

void FuchsiaCdm::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  NOTIMPLEMENTED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "not implemented");
}

void FuchsiaCdm::LoadSession(CdmSessionType session_type,
                             const std::string& session_id,
                             std::unique_ptr<NewSessionCdmPromise> promise) {
  NOTIMPLEMENTED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "not implemented");
}

void FuchsiaCdm::UpdateSession(const std::string& session_id,
                               const std::vector<uint8_t>& response,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  NOTIMPLEMENTED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "not implemented");
}

void FuchsiaCdm::CloseSession(const std::string& session_id,
                              std::unique_ptr<SimpleCdmPromise> promise) {
  NOTIMPLEMENTED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "not implemented");
}

void FuchsiaCdm::RemoveSession(const std::string& session_id,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  NOTIMPLEMENTED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "not implemented");
}

CdmContext* FuchsiaCdm::GetCdmContext() {
  return this;
}

std::unique_ptr<CallbackRegistration> FuchsiaCdm::RegisterEventCB(
    EventCB event_cb) {
  NOTIMPLEMENTED();
  return nullptr;
}

Decryptor* FuchsiaCdm::GetDecryptor() {
  NOTIMPLEMENTED();
  return nullptr;
}

int FuchsiaCdm::GetCdmId() const {
  return kInvalidCdmId;
}

}  // namespace media
