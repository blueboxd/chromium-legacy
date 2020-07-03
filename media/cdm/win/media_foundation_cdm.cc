// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/media_foundation_cdm.h"

#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_promise.h"
#include "media/base/win/mf_cdm_proxy.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/media_foundation_cdm_session.h"

namespace media {

namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Exception = CdmPromise::Exception;

class CdmProxyImpl
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFCdmProxy> {
 public:
  CdmProxyImpl() = default;
  ~CdmProxyImpl() override = default;

  HRESULT RuntimeClassInitialize(ComPtr<IMFContentDecryptionModule> mf_cdm) {
    mf_cdm_ = mf_cdm;
    return S_OK;
  }

  // IMFCdmProxy implementation

  STDMETHODIMP GetPMPServer(REFIID riid, LPVOID* object_result) override {
    ComPtr<IMFGetService> cdm_services;
    RETURN_IF_FAILED(mf_cdm_.As(&cdm_services));
    RETURN_IF_FAILED(cdm_services->GetService(
        MF_CONTENTDECRYPTIONMODULE_SERVICE, riid, object_result));
    return S_OK;
  }

  STDMETHODIMP GetInputTrustAuthority(uint64_t playback_element_id,
                                      uint32_t stream_id,
                                      uint32_t /*stream_count*/,
                                      const uint8_t* content_init_data,
                                      uint32_t content_init_data_size,
                                      REFIID riid,
                                      IUnknown** object_out) override {
    DVLOG_FUNC(1);

    if (input_trust_authorities_.count(stream_id)) {
      RETURN_IF_FAILED(input_trust_authorities_[stream_id].CopyTo(object_out));
      return S_OK;
    }

    ComPtr<IMFTrustedInput> trusted_input;
    RETURN_IF_FAILED(mf_cdm_->CreateTrustedInput(
        content_init_data, content_init_data_size, &trusted_input));

    // GetInputTrustAuthority takes IUnknown* as the output. Using other COM
    // interface will have a v-table mismatch issue.
    ComPtr<IUnknown> unknown;
    RETURN_IF_FAILED(
        trusted_input->GetInputTrustAuthority(stream_id, riid, &unknown));

    ComPtr<IMFInputTrustAuthority> input_trust_authority;
    RETURN_IF_FAILED(unknown.As(&input_trust_authority));
    RETURN_IF_FAILED(unknown.CopyTo(object_out));

    // Success! Store ITA in the map.
    input_trust_authorities_[stream_id] = input_trust_authority;

    return S_OK;
  }

  // TODO(xhwang): Implement this.
  STDMETHODIMP RefreshTrustedInput(uint64_t playback_element_id) override {
    NOTIMPLEMENTED();
    return S_OK;
  }

  // TODO(xhwang): Implement this.
  STDMETHODIMP SetLastKeyIds(uint64_t playback_element_id,
                             GUID* key_ids,
                             uint32_t key_ids_count) override {
    NOTIMPLEMENTED();
    return S_OK;
  }

  STDMETHODIMP
  ProcessContentEnabler(IUnknown* request, IMFAsyncResult* result) override {
    ComPtr<IMFContentEnabler> content_enabler;
    RETURN_IF_FAILED(request->QueryInterface(IID_PPV_ARGS(&content_enabler)));
    return mf_cdm_->SetContentEnabler(content_enabler.Get(), result);
  }

 private:
  ComPtr<IMFContentDecryptionModule> mf_cdm_;

  // |stream_id| to IMFInputTrustAuthority (ITA) mapping. Serves two purposes:
  // 1. The same ITA should always be returned in GetInputTrustAuthority() for
  // the same |stream_id|.
  // 2. The ITA must keep alive for decryptors to work.
  std::map<uint32_t, ComPtr<IMFInputTrustAuthority>> input_trust_authorities_;
};

}  // namespace

MediaFoundationCdm::MediaFoundationCdm(
    Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mf_cdm,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb)
    : mf_cdm_(std::move(mf_cdm)),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {
  DVLOG_FUNC(1);
  DCHECK(mf_cdm_);
  DCHECK(session_message_cb_);
  DCHECK(session_closed_cb_);
  DCHECK(session_keys_change_cb_);
  DCHECK(session_expiration_update_cb_);
}

MediaFoundationCdm::~MediaFoundationCdm() {
  DVLOG_FUNC(1);
}

void MediaFoundationCdm::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  if (FAILED(mf_cdm_->SetServerCertificate(certificate.data(),
                                           certificate.size()))) {
    promise->reject(Exception::NOT_SUPPORTED_ERROR, 0, "Failed to set cert");
    return;
  }

  promise->resolve();
}

// TODO(xhwang): Implement this.
void MediaFoundationCdm::GetStatusForPolicy(
    HdcpVersion min_hdcp_version,
    std::unique_ptr<KeyStatusCdmPromise> promise) {
  NOTIMPLEMENTED();
  promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                  "GetStatusForPolicy() is not supported.");
}

void MediaFoundationCdm::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG_FUNC(1);

  // TODO(xhwang): Implement session expiration update.
  auto session = std::make_unique<MediaFoundationCdmSession>(
      session_message_cb_, session_keys_change_cb_);

  if (FAILED(session->Initialize(mf_cdm_.Get(), session_type))) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0,
                    "Failed to create session");
    return;
  }

  int session_token = next_session_token_++;

  // Keep a raw pointer since the |promise| will be moved to the callback.
  auto* raw_promise = promise.get();
  auto session_id_cb = base::BindOnce(&MediaFoundationCdm::OnSessionId,
                                      weak_factory_.GetWeakPtr(), session_token,
                                      std::move(promise));

  if (FAILED(session->GenerateRequest(init_data_type, init_data,
                                      std::move(session_id_cb)))) {
    raw_promise->reject(Exception::INVALID_STATE_ERROR, 0, "Init failure");
    return;
  }

  pending_sessions_.emplace(session_token, std::move(session));
}

void MediaFoundationCdm::LoadSession(
    CdmSessionType session_type,
    const std::string& session_id,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG_FUNC(1);
  NOTIMPLEMENTED();
  promise->reject(Exception::NOT_SUPPORTED_ERROR, 0, "Load not supported");
}

void MediaFoundationCdm::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  auto* session = GetSession(session_id);
  if (!session) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Session not found");
    return;
  }

  if (FAILED(session->Update(response))) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Update failed");
    return;
  }

  promise->resolve();
}

void MediaFoundationCdm::CloseSession(
    const std::string& session_id,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  // Validate that this is a reference to an open session. close() shouldn't
  // be called if the session is already closed. However, the operation is
  // asynchronous, so there is a window where close() was called a second time
  // just before the closed event arrives. As a result it is possible that the
  // session is already closed, so assume that the session is closed if it
  // doesn't exist. https://github.com/w3c/encrypted-media/issues/365.
  //
  // close() is called from a MediaKeySession object, so it is unlikely that
  // this method will be called with a previously unseen |session_id|.
  auto* session = GetSession(session_id);
  if (!session) {
    promise->resolve();
    return;
  }

  if (FAILED(session->Close())) {
    sessions_.erase(session_id);
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Close failed");
    return;
  }

  // EME requires running session closed algorithm before resolving the promise.
  sessions_.erase(session_id);
  session_closed_cb_.Run(session_id);
  promise->resolve();
}

void MediaFoundationCdm::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG_FUNC(1);

  auto* session = GetSession(session_id);
  if (!session) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Session not found");
    return;
  }

  if (FAILED(session->Remove())) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0, "Remove failed");
    return;
  }

  promise->resolve();
}

CdmContext* MediaFoundationCdm::GetCdmContext() {
  return this;
}

bool MediaFoundationCdm::GetMediaFoundationCdmProxy(
    GetMediaFoundationCdmProxyCB get_mf_cdm_proxy_cb) {
  DVLOG_FUNC(1);

  if (!cdm_proxy_)
    MakeAndInitialize<CdmProxyImpl>(&cdm_proxy_, mf_cdm_);

  BindToCurrentLoop(std::move(get_mf_cdm_proxy_cb)).Run(cdm_proxy_);
  return true;
}

void MediaFoundationCdm::OnSessionId(
    int session_token,
    std::unique_ptr<NewSessionCdmPromise> promise,
    const std::string& session_id) {
  DVLOG_FUNC(1) << "session_token=" << session_token
                << ", session_id=" << session_id;

  auto itr = pending_sessions_.find(session_token);
  DCHECK(itr != pending_sessions_.end());
  auto session = std::move(itr->second);
  DCHECK(session);
  pending_sessions_.erase(itr);

  if (session_id.empty() || sessions_.count(session_id)) {
    promise->reject(Exception::INVALID_STATE_ERROR, 0,
                    "Empty or duplicate session ID");
    return;
  }

  sessions_.emplace(session_id, std::move(session));
  promise->resolve(session_id);
}

MediaFoundationCdmSession* MediaFoundationCdm::GetSession(
    const std::string& session_id) {
  auto itr = sessions_.find(session_id);
  if (itr == sessions_.end())
    return nullptr;

  return itr->second.get();
}

}  // namespace media
