// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_

#include "base/android/jni_android.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace upboarding {

// Helper class responsible for bridging the TileProvider between C++ and Java.
class TileProviderBridge {
 public:
  TileProviderBridge();
  ~TileProviderBridge();

  // Methods called from Java via JNI.
  void GetQueryTiles(JNIEnv* env,
                     const JavaParamRef<jobject>& jcaller,
                     const JavaParamRef<jobject>& jcallback);

  void GetThumbnail(JNIEnv* env,
                    const JavaParamRef<jobject>& jcaller,
                    const JavaParamRef<jstring>& jid,
                    const JavaParamRef<jobject>& jcallback);

 private:
  // A reference to the Java counterpart of this class.  See
  // TileProviderBridge.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(TileProviderBridge);
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_
