// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_BADGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_BADGE_H_

#include "base/optional.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/badging/badging.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class BadgeOptions;
class ExceptionState;
class ExecutionContext;
class ScriptState;

class Badge final : public ScriptWrappable,
                    public Supplement<ExecutionContext> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Badge);

 public:
  static const char kSupplementName[];

  static Badge* From(ExecutionContext*);

  explicit Badge(ExecutionContext*);
  ~Badge() override;

  // Badge IDL interface.
  static void set(ScriptState*, const BadgeOptions*, ExceptionState&);
  static void set(ScriptState*,
                  uint64_t content,
                  const BadgeOptions*,
                  ExceptionState&);
  static void set(ScriptState*, ExceptionState&);
  static void set(ScriptState*, uint64_t content, ExceptionState&);
  static void clear(ScriptState*, const BadgeOptions*, ExceptionState&);

  void SetBadge(WTF::String scope,
                mojom::blink::BadgeValuePtr value,
                ExceptionState&);
  void ClearBadge(WTF::String scope, ExceptionState&);

  void Trace(blink::Visitor*) override;

 private:
  static Badge* BadgeFromState(ScriptState* script_state);

  // If the URL is invalid, sets an exception and returns nullopt, which callers
  // should check for and stop doing work.
  base::Optional<KURL> ScopeStringToURL(WTF::String& scope, ExceptionState&);

  mojo::Remote<blink::mojom::blink::BadgeService> badge_service_;
  Member<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_BADGE_H_
