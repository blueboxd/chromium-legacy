// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_SESSION_H_
#define MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_SESSION_H_

#include <mfcontentdecryptionmodule.h>
#include <wrl.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"

namespace media {

// A class wrapping IMFContentDecryptionModuleSession.
class MEDIA_EXPORT MediaFoundationCdmSession {
 public:
  MediaFoundationCdmSession(const SessionMessageCB& session_message_cb,
                            const SessionKeysChangeCB& session_keys_change_cb);
  MediaFoundationCdmSession(const MediaFoundationCdmSession&) = delete;
  MediaFoundationCdmSession& operator=(const MediaFoundationCdmSession&) =
      delete;
  ~MediaFoundationCdmSession();

  // Initializes the session. All other methods should only be called after
  // Initialize() returns S_OK.
  HRESULT Initialize(IMFContentDecryptionModule* mf_cdm,
                     CdmSessionType session_type);

  // EME MediaKeySession methods. Returns S_OK on success, otherwise forwards
  // the HRESULT from IMFContentDecryptionModuleSession.
  // Note on GenerateRequest():
  // - Returns S_OK, which has two cases:
  //   * If |session_id_| is successfully set, |session_id_cb| will be run
  //     followed by the session message.
  //   * Otherwise, |session_id_cb| will be run with an empty session ID to
  //     indicate error. No session message in this case.
  // - Otherwise, no callbacks will be run.
  using SessionIdCB = base::OnceCallback<void(const std::string&)>;
  HRESULT GenerateRequest(EmeInitDataType init_data_type,
                          const std::vector<uint8_t>& init_data,
                          SessionIdCB session_id_cb);
  HRESULT Load(const std::string& session_id);
  HRESULT Update(const std::vector<uint8_t>& response);
  HRESULT Close();
  HRESULT Remove();

 private:
  // Callbacks for forwarding session events.
  void OnSessionMessage(CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionKeysChange();

  // Sets |session_id_|, which could be empty on failure.
  void SetSessionId();

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionKeysChangeCB session_keys_change_cb_;

  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSession> mf_cdm_session_;

  // Callback passed in GenerateRequest() to return the session ID.
  SessionIdCB session_id_cb_;

  std::string session_id_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationCdmSession> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_SESSION_H_
