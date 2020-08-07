// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_BROWSER_ON_LOAD_SCRIPT_INJECTOR_HOST_H_
#define COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_BROWSER_ON_LOAD_SCRIPT_INJECTOR_HOST_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/safe_math.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/on_load_script_injector/export.h"
#include "components/on_load_script_injector/on_load_script_injector.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/origin.h"

namespace on_load_script_injector {

class ON_LOAD_SCRIPT_INJECTOR_EXPORT OriginScopedScript {
 public:
  OriginScopedScript();
  OriginScopedScript(std::vector<url::Origin> origins,
                     base::ReadOnlySharedMemoryRegion script);
  OriginScopedScript& operator=(OriginScopedScript&& other);
  ~OriginScopedScript();

  const std::vector<url::Origin>& origins() const { return origins_; }
  const base::ReadOnlySharedMemoryRegion& script() const { return script_; }

 private:
  std::vector<url::Origin> origins_;

  // A shared memory buffer containing the script, encoded as UTF16.
  base::ReadOnlySharedMemoryRegion script_;
};

// Manages the set of scripts to be injected into document just prior to
// document load.
template <typename ScriptId>
class OnLoadScriptInjectorHost {
 public:
  OnLoadScriptInjectorHost() = default;
  ~OnLoadScriptInjectorHost() = default;

  OnLoadScriptInjectorHost(const OnLoadScriptInjectorHost&) = delete;
  OnLoadScriptInjectorHost& operator=(const OnLoadScriptInjectorHost&) = delete;

  // Adds a |script| to be injected on pages whose URL's origin matches at least
  // one entry of |origins_to_inject|.
  // Scripts will be loaded in the order they are added.
  // If a script with |id| already exists, it will be replaced with the original
  // sequence position preserved.
  // All entries of |origins_to_inject| must be valid/not opaque.
  void AddScript(ScriptId id,
                 std::vector<url::Origin> origins_to_inject,
                 base::StringPiece script) {
    // If there is no script with the identifier |id|, then create a place for
    // it at the end of the injection sequence.
    if (before_load_scripts_.find(id) == before_load_scripts_.end())
      before_load_scripts_order_.push_back(id);

    // Convert script to UTF-16.
    base::string16 script_utf16 = base::UTF8ToUTF16(script);
    size_t script_utf16_size =
        (base::CheckedNumeric<size_t>(script_utf16.size()) *
         sizeof(base::char16))
            .ValueOrDie();
    base::WritableSharedMemoryRegion script_shared_memory =
        base::WritableSharedMemoryRegion::Create(script_utf16_size);
    memcpy(script_shared_memory.Map().memory(), script_utf16.data(),
           script_utf16_size);

    base::ReadOnlySharedMemoryRegion script_shared_memory_readonly =
        base::WritableSharedMemoryRegion::ConvertToReadOnly(
            std::move(script_shared_memory));
    CHECK(script_shared_memory_readonly.IsValid());

    before_load_scripts_[id] = OriginScopedScript(
        origins_to_inject, std::move(script_shared_memory_readonly));
  }

  // Same as AddScript(), except that scripts are injected for all pages.
  void AddScriptForAllOrigins(ScriptId id, base::StringPiece script) {
    AddScript(id, {kMatchAllOrigins}, script);
  }

  // Removes the script |id|.
  void RemoveScript(ScriptId id) {
    before_load_scripts_.erase(id);

    for (auto script_id_iter = before_load_scripts_order_.begin();
         script_id_iter != before_load_scripts_order_.end(); ++script_id_iter) {
      if (*script_id_iter == id) {
        before_load_scripts_order_.erase(script_id_iter);
        return;
      }
    }

    LOG(WARNING) << "Ignoring attempt to remove unknown OnLoad script: " << id;
  }

  // Injects the scripts associated with the origin of |url| into the document
  // hosted by |render_frame_host|.
  void InjectScriptsForURL(const GURL& url,
                           content::RenderFrameHost* render_frame_host) {
    DCHECK(url.is_valid());

    mojo::AssociatedRemote<mojom::OnLoadScriptInjector> injector;
    render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&injector);

    injector->ClearOnLoadScripts();

    if (before_load_scripts_.empty())
      return;

    // Provision the renderer's ScriptInjector with the scripts associated with
    // |url|.
    for (ScriptId script_id : before_load_scripts_order_) {
      const OriginScopedScript& script = before_load_scripts_[script_id];
      if (IsUrlMatchedByOriginList(url, script.origins()))
        injector->AddOnLoadScript(script.script().Duplicate());
    }
  }

 private:
  bool IsUrlMatchedByOriginList(
      const GURL& url,
      const std::vector<url::Origin>& allowed_origins) {
    url::Origin url_origin = url::Origin::Create(url);

    for (const url::Origin& allowed_origin : allowed_origins) {
      if (allowed_origin == kMatchAllOrigins)
        return true;

      DCHECK(!allowed_origin.opaque());
      if (url_origin.IsSameOriginWith(allowed_origin))
        return true;
    }

    return false;
  }

  // An opaque Origin that, when specified, allows script injection on all URLs
  // regardless of origin.
  const url::Origin kMatchAllOrigins;

  std::map<ScriptId, OriginScopedScript> before_load_scripts_;
  std::vector<ScriptId> before_load_scripts_order_;
};

}  // namespace on_load_script_injector

#endif  // COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_BROWSER_ON_LOAD_SCRIPT_INJECTOR_HOST_
