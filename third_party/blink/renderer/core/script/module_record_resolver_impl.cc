// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/module_record_resolver_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"

namespace blink {

void ModuleRecordResolverImpl::RegisterModuleScript(
    const ModuleScript* module_script) {
  DCHECK(module_script);
  if (module_script->Record().IsNull())
    return;

  DVLOG(1) << "ModuleRecordResolverImpl::RegisterModuleScript(url="
           << module_script->BaseURL().GetString()
           << ", hash=" << ModuleRecordHash::GetHash(module_script->Record())
           << ")";

  auto result =
      record_to_module_script_map_.Set(module_script->Record(), module_script);
  DCHECK(result.is_new_entry);
}

void ModuleRecordResolverImpl::UnregisterModuleScript(
    const ModuleScript* module_script) {
  DCHECK(module_script);
  if (module_script->Record().IsNull())
    return;

  DVLOG(1) << "ModuleRecordResolverImpl::UnregisterModuleScript(url="
           << module_script->BaseURL().GetString()
           << ", hash=" << ModuleRecordHash::GetHash(module_script->Record())
           << ")";

  record_to_module_script_map_.erase(module_script->Record());
}

const ModuleScript* ModuleRecordResolverImpl::GetModuleScriptFromModuleRecord(
    const ModuleRecord& record) const {
  const auto it = record_to_module_script_map_.find(record);
  CHECK_NE(it, record_to_module_script_map_.end())
      << "Failed to find ModuleScript corresponding to the "
         "record.[[HostDefined]]";
  CHECK(it->value);
  return it->value;
}

// <specdef
// href="https://html.spec.whatwg.org/C/#hostresolveimportedmodule(referencingscriptormodule,-specifier)">
ModuleRecord ModuleRecordResolverImpl::Resolve(
    const String& specifier,
    const ModuleRecord& referrer,
    ExceptionState& exception_state) {
  DVLOG(1) << "ModuleRecordResolverImpl::resolve(specifier=\"" << specifier
           << ", referrer.hash=" << ModuleRecordHash::GetHash(referrer) << ")";
  // <spec step="3">If referencingScriptOrModule is not null, then:</spec>
  //
  // Currently this function implements the spec before
  // https://github.com/tc39/proposal-dynamic-import is applied, i.e. where
  // |referencingScriptOrModule| was always a non-null module script.

  // <spec step="3.2">Set settings object to referencing script's settings
  // object.</spec>
  //
  // <spec step="4">Let moduleMap be settings object's module map.</spec>
  //
  // These are |modulator_| and |this|, respectively, because module script's
  // settings object is always the current settings object in Blink.

  // <spec step="3.1">Let referencing script be
  // referencingScriptOrModule.[[HostDefined]].</spec>
  const ModuleScript* referrer_module =
      GetModuleScriptFromModuleRecord(referrer);

  // <spec step="3.3">Set base URL to referencing script's base URL.</spec>
  // <spec step="5">Let url be the result of resolving a module specifier given
  // base URL and specifier.</spec>
  KURL url = referrer_module->ResolveModuleSpecifier(specifier);

  // <spec step="6">Assert: url is never failure, because resolving a module
  // specifier must have been previously successful with these same two
  // arguments ...</spec>
  DCHECK(url.IsValid());

  // <spec step="7">Let resolved module script be moduleMap[url]. (This entry
  // must exist for us to have gotten to this point.)</spec>
  ModuleScript* module_script = modulator_->GetFetchedModuleScript(url);

  // <spec step="8">Assert: resolved module script is a module script (i.e., is
  // not null or "fetching").</spec>
  //
  // <spec step="9">Assert: resolved module script's record is not null.</spec>
  DCHECK(module_script);
  CHECK(!module_script->Record().IsNull());

  // <spec step="10">Return resolved module script's record.</spec>
  return module_script->Record();
}

void ModuleRecordResolverImpl::ContextDestroyed(ExecutionContext*) {
  // crbug.com/725816 : What we should really do is to make the map key
  // weak reference to v8::Module.
  record_to_module_script_map_.clear();
}

void ModuleRecordResolverImpl::Trace(Visitor* visitor) {
  ModuleRecordResolver::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(record_to_module_script_map_);
  visitor->Trace(modulator_);
}

}  // namespace blink
