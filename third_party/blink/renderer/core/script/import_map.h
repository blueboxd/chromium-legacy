// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_IMPORT_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_IMPORT_MAP_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class ConsoleLogger;
class JSONObject;
class Modulator;
class ParsedSpecifier;
class ScriptValue;

// Import maps.
// https://wicg.github.io/import-maps/
// https://github.com/WICG/import-maps/blob/master/spec.md
class CORE_EXPORT ImportMap final : public GarbageCollected<ImportMap> {
 public:
  static ImportMap* Parse(const Modulator&,
                          const String& text,
                          const KURL& base_url,
                          ConsoleLogger& logger,
                          ScriptValue* error_to_rethrow);

  ImportMap(const Modulator&, const HashMap<String, Vector<KURL>>& imports);

  // https://wicg.github.io/import-maps/#resolve-an-imports-match
  // Returns nullopt when not mapped by |this| import map (i.e. the import map
  // doesn't have corresponding keys).
  // Returns a null URL when resolution fails.
  base::Optional<KURL> ResolveImportsMatch(const ParsedSpecifier&,
                                           String* debug_message) const;

  String ToString() const;

  void Trace(Visitor*);

 private:
  // <spec href="https://wicg.github.io/import-maps/#specifier-map">A specifier
  // map is an ordered map from strings to lists of URLs.</spec>
  //
  // In Blink, we actually use an unordered map here, and related algorithms
  // are implemented differently from the spec.
  using SpecifierMap = HashMap<String, Vector<KURL>>;

  using MatchResult = SpecifierMap::const_iterator;

  base::Optional<MatchResult> MatchPrefix(const ParsedSpecifier&) const;
  static SpecifierMap SortAndNormalizeSpecifierMap(const JSONObject* imports,
                                                   const KURL& base_url,
                                                   ConsoleLogger&);

  base::Optional<KURL> ResolveImportsMatchInternal(
      const String& normalizedSpecifier,
      const MatchResult&,
      String* debug_message) const;

  // https://wicg.github.io/import-maps/#import-map-imports
  SpecifierMap imports_;

  // TODO(crbug.com/927181): Implement
  // https://wicg.github.io/import-maps/#import-map-scopes.

  Member<const Modulator> modulator_for_built_in_modules_;
};

}  // namespace blink

#endif
