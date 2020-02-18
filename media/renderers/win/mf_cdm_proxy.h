// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MF_CDM_PROXY_H_
#define MEDIA_RENDERERS_WIN_MF_CDM_PROXY_H_

#include <stdint.h>
#include <unknwn.h>

// Interface for clients to get information from MediaFoundationCdm to
// implements COM interfaces.
// COM interface is used because we are working with Media Foundation which uses
// COM extensively for object lifetime management.
MIDL_INTERFACE("565ab5c2-9923-44e0-997a-f93ccba5dcbf")
IMFCdmProxy : public IUnknown {
 public:
  // Used by MediaFoundationProtectionManager to get
  // ABI::Windows::Media::Protection::IMediaProtectionPMPServer to implement
  // ABI::Windows::Media::Protection::IMediaProtectionManager::get_Properties as
  // in
  // https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.mediaprotectionmanager
  virtual HRESULT STDMETHODCALLTYPE GetPMPServer(
      /* [in] */ __RPC__in REFIID riid,
      /* [iid_is][out] */ __RPC__deref_out_opt LPVOID * object_result) = 0;

  // Used by MediaFoundationSourceWrapper to implement
  // IMFTrustedInput::GetInputTrustAuthority as in
  // https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imftrustedinput
  //
  // |content_init_data| is optional initialization data as in
  // https://www.w3.org/TR/encrypted-media/#initialization-data
  virtual HRESULT STDMETHODCALLTYPE GetInputTrustAuthority(
      _In_ uint64_t playback_element_id, _In_ uint32_t stream_id,
      _In_ uint32_t stream_count,
      _In_reads_bytes_opt_(content_init_data_size)
          const uint8_t* content_init_data,
      _In_ uint32_t content_init_data_size, _In_ REFIID riid,
      _COM_Outptr_ IUnknown** object_out) = 0;

  // MediaFoundationSourceWrapper provides its last set of key ids
  // associated with a playback element id using SetLastKeyIds when it is
  // destructed.
  // Another instance of MediaFoundationSourceWrapper could then invoke
  // RefreshTrustedInput to let implementation to reuse those key ids
  // information when it happens to have the same playback element id.
  //
  // |playback_element_id| is an ID corresponding to a particular instance of
  // video playback element.
  virtual HRESULT STDMETHODCALLTYPE RefreshTrustedInput(
      _In_ uint64_t playback_element_id) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetLastKeyIds(
      _In_ uint64_t playback_element_id, GUID * key_ids,
      uint32_t key_ids_count) = 0;

  // Used by MediaFoundationProtectionManager to implement
  // IMFContentProtectionManager::BeginEnableContent as in
  // https://msdn.microsoft.com/en-us/windows/ms694217(v=vs.71)
  //
  // |result| is used to obtain the result of an asynchronous operation as in
  // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nn-mfobjects-imfasyncresult
  virtual HRESULT STDMETHODCALLTYPE ProcessContentEnabler(
      _In_ IUnknown * request, _In_ IMFAsyncResult * result) = 0;
};

#endif  // MEDIA_RENDERERS_WIN_MF_CDM_PROXY_H_
