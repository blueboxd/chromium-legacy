// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/web_applications/manifest_request_filter.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"

namespace web_app {

void SetManifestRequestFilter(content::WebUIDataSource* source,
                              int manifest_idr,
                              int name_ids) {
  ui::TemplateReplacements replacements;
  base::string16 name = l10n_util::GetStringUTF16(name_ids);
  base::ReplaceChars(name, base::ASCIIToUTF16("\""), base::ASCIIToUTF16("\\\""),
                     &name);
  replacements["name"] = base::UTF16ToUTF8(name);

  scoped_refptr<base::RefCountedMemory> bytes =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          manifest_idr);
  base::StringPiece content(reinterpret_cast<const char*>(bytes->front()),
                            bytes->size());
  std::string response = ui::ReplaceTemplateExpressions(content, replacements);

  source->SetRequestFilter(
      base::BindRepeating(
          [](const std::string& path) { return path == "manifest.json"; }),
      base::BindRepeating(
          [](const std::string& response, const std::string& path,
             content::WebUIDataSource::GotDataCallback callback) {
            std::string response_copy = response;
            std::move(callback).Run(
                base::RefCountedString::TakeString(&response_copy));
          },
          std::move(response)));
}

}  // namespace web_app
