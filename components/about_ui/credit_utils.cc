// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/about_ui/credit_utils.h"

#include <stdint.h>

#include "base/strings/string_piece.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "base/files/file.h"
#include "components/about_ui/android/about_ui_jni_headers/CreditUtils_jni.h"
#endif

namespace about_ui {

std::string GetCredits(bool include_scripts) {
  std::string response =
      ui::ResourceBundle::GetSharedInstance().DecompressDataResource(
          IDR_ABOUT_UI_CREDITS_HTML);
  if (include_scripts) {
    response +=
        "\n<script src=\"chrome://resources/js/cr.js\"></script>\n"
        "<script src=\"chrome://credits/credits.js\"></script>\n";
  }
  response += "</body>\n</html>";
  return response;
}

#if defined(OS_ANDROID)
static void JNI_CreditUtils_WriteCreditsHtml(JNIEnv* env, jint fd) {
  std::string html_content = GetCredits(false);
  base::File out_file(fd);
  out_file.WriteAtCurrentPos(html_content.c_str(), html_content.size());
}
#endif

}  // namespace about_ui
