// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_certificate_generator.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/render_thread_impl.h"
#include "media/media_buildflags.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/rtc_certificate.h"
#include "third_party/webrtc/rtc_base/rtc_certificate_generator.h"
#include "url/gurl.h"

namespace content {
namespace {

rtc::KeyParams WebRTCKeyParamsToKeyParams(
    const blink::WebRTCKeyParams& key_params) {
  switch (key_params.KeyType()) {
    case blink::kWebRTCKeyTypeRSA:
      return rtc::KeyParams::RSA(key_params.RsaParams().mod_length,
                                 key_params.RsaParams().pub_exp);
    case blink::kWebRTCKeyTypeECDSA:
      return rtc::KeyParams::ECDSA(
          static_cast<rtc::ECCurve>(key_params.EcCurve()));
    default:
      NOTREACHED();
      return rtc::KeyParams();
  }
}

// A certificate generation request spawned by
// |GenerateCertificateWithOptionalExpiration|. This
// is handled by a separate class so that reference counting can keep the
// request alive independently of the |RTCCertificateGenerator| that spawned it.
class RTCCertificateGeneratorRequest
    : public base::RefCountedThreadSafe<RTCCertificateGeneratorRequest> {
 public:
  RTCCertificateGeneratorRequest(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& worker_thread)
      : main_thread_(main_thread),
        worker_thread_(worker_thread) {
    DCHECK(main_thread_);
    DCHECK(worker_thread_);
  }

  void GenerateCertificateAsync(
      const blink::WebRTCKeyParams& key_params,
      const absl::optional<uint64_t>& expires_ms,
      blink::WebRTCCertificateCallback completion_callback) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(completion_callback);

    worker_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCCertificateGeneratorRequest::GenerateCertificateOnWorkerThread,
            this, key_params, expires_ms, std::move(completion_callback)));
  }

 private:
  friend class base::RefCountedThreadSafe<RTCCertificateGeneratorRequest>;
  ~RTCCertificateGeneratorRequest() {}

  void GenerateCertificateOnWorkerThread(
      const blink::WebRTCKeyParams key_params,
      const absl::optional<uint64_t> expires_ms,
      blink::WebRTCCertificateCallback completion_callback) {
    DCHECK(worker_thread_->BelongsToCurrentThread());

    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificateGenerator::GenerateCertificate(
            WebRTCKeyParamsToKeyParams(key_params), expires_ms);

    main_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&RTCCertificateGeneratorRequest::DoCallbackOnMainThread,
                       this, std::move(completion_callback), certificate));
  }

  void DoCallbackOnMainThread(
      blink::WebRTCCertificateCallback completion_callback,
      rtc::scoped_refptr<rtc::RTCCertificate> certificate) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(completion_callback);
    std::move(completion_callback).Run(std::move(certificate));
  }

  // The main thread is the renderer thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  // The WebRTC worker thread.
  const scoped_refptr<base::SingleThreadTaskRunner> worker_thread_;
};

void GenerateCertificateWithOptionalExpiration(
    const blink::WebRTCKeyParams& key_params,
    const absl::optional<uint64_t>& expires_ms,
    blink::WebRTCCertificateCallback completion_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(WebRTCKeyParamsToKeyParams(key_params).IsValid());
  PeerConnectionDependencyFactory* pc_dependency_factory =
      RenderThreadImpl::current()->GetPeerConnectionDependencyFactory();
  pc_dependency_factory->EnsureInitialized();

  scoped_refptr<RTCCertificateGeneratorRequest> request =
      new RTCCertificateGeneratorRequest(
          task_runner, pc_dependency_factory->GetWebRtcWorkerThread());
  request->GenerateCertificateAsync(key_params, expires_ms,
                                    std::move(completion_callback));
}

}  // namespace

void RTCCertificateGenerator::GenerateCertificate(
    const blink::WebRTCKeyParams& key_params,
    blink::WebRTCCertificateCallback completion_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  GenerateCertificateWithOptionalExpiration(
      key_params, absl::nullopt, std::move(completion_callback), task_runner);
}

void RTCCertificateGenerator::GenerateCertificateWithExpiration(
    const blink::WebRTCKeyParams& key_params,
    uint64_t expires_ms,
    blink::WebRTCCertificateCallback completion_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  GenerateCertificateWithOptionalExpiration(
      key_params, expires_ms, std::move(completion_callback), task_runner);
}

bool RTCCertificateGenerator::IsSupportedKeyParams(
    const blink::WebRTCKeyParams& key_params) {
  return WebRTCKeyParamsToKeyParams(key_params).IsValid();
}

rtc::scoped_refptr<rtc::RTCCertificate> RTCCertificateGenerator::FromPEM(
    blink::WebString pem_private_key,
    blink::WebString pem_certificate) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::FromPEM(rtc::RTCCertificatePEM(
          pem_private_key.Utf8(), pem_certificate.Utf8()));
  if (!certificate)
    return nullptr;
  return certificate;
}

}  // namespace content
