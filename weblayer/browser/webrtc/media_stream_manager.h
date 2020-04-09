// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBRTC_MEDIA_STREAM_MANAGER_H_
#define WEBLAYER_BROWSER_WEBRTC_MEDIA_STREAM_MANAGER_H_

#include <set>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/media_stream_request.h"

namespace content {
class WebContents;
}

namespace weblayer {

// On Android, this class tracks active media streams and updates the Java
// object of the same name as streams come and go. The class is created and
// destroyed by the Java object.
class MediaStreamManager {
 public:
  // It's expected that |j_web_contents| outlasts |this|.
  MediaStreamManager(
      const base::android::JavaParamRef<jobject>& j_object,
      const base::android::JavaParamRef<jobject>& j_web_contents);
  MediaStreamManager(const MediaStreamManager&) = delete;
  MediaStreamManager& operator=(const MediaStreamManager&) = delete;
  ~MediaStreamManager();

  static MediaStreamManager* FromWebContents(content::WebContents* contents);

  // Requests media access permission for the tab, if necessary, and runs
  // |callback| as appropriate. This will create a StreamUi.
  void RequestMediaAccessPermission(const content::MediaStreamRequest& request,
                                    content::MediaResponseCallback callback);

 private:
  class StreamUi;

  void OnMediaAccessPermissionResult(
      content::MediaResponseCallback callback,
      const blink::MediaStreamDevices& devices,
      blink::mojom::MediaStreamRequestResult result,
      bool blocked_by_feature_policy,
      ContentSetting audio_setting,
      ContentSetting video_setting);

  void RegisterStream(StreamUi* stream);
  void UnregisterStream(StreamUi* stream);
  void Update();

  std::set<StreamUi*> active_streams_;

  base::android::ScopedJavaGlobalRef<jobject> j_object_;

  base::WeakPtrFactory<MediaStreamManager> weak_factory_{this};
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBRTC_MEDIA_STREAM_MANAGER_H_
