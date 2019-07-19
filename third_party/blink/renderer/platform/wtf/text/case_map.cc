#include "third_party/blink/renderer/platform/wtf/text/case_map.h"

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

namespace {

inline bool LocaleIdMatchesLang(const AtomicString& locale_id,
                                const StringView& lang) {
  CHECK_GE(lang.length(), 2u);
  CHECK_LE(lang.length(), 3u);
  if (!locale_id.Impl() || !locale_id.Impl()->StartsWithIgnoringCase(lang))
    return false;
  if (locale_id.Impl()->length() == lang.length())
    return true;
  const UChar maybe_delimiter = (*locale_id.Impl())[lang.length()];
  return maybe_delimiter == '-' || maybe_delimiter == '_' ||
         maybe_delimiter == '@';
}

typedef int32_t (*icuCaseConverter)(UChar*,
                                    int32_t,
                                    const UChar*,
                                    int32_t,
                                    const char*,
                                    UErrorCode*);

scoped_refptr<StringImpl> CaseConvert(icuCaseConverter converter,
                                      StringImpl* source,
                                      const char* locale) {
  CHECK_LE(source->length(),
           static_cast<wtf_size_t>(std::numeric_limits<int32_t>::max()));
  const wtf_size_t source_length = source->length();

  scoped_refptr<StringImpl> upconverted = source->UpconvertedString();
  const UChar* source16 = upconverted->Characters16();

  UChar* data16;
  wtf_size_t target_length = source_length;
  scoped_refptr<StringImpl> output =
      StringImpl::CreateUninitialized(target_length, data16);
  while (true) {
    UErrorCode status = U_ZERO_ERROR;
    target_length = converter(data16, target_length, source16, source_length,
                              locale, &status);
    if (U_SUCCESS(status)) {
      if (source_length == target_length)
        return output;
      return output->Substring(0, target_length);
    }

    // Expand the buffer and retry if the target is longer.
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      output = StringImpl::CreateUninitialized(target_length, data16);
      continue;
    }

    NOTREACHED();
    return source;
  }
}

}  // namespace

const char* CaseMap::Locale::turkic_or_azeri_ = "tr";
const char* CaseMap::Locale::greek_ = "el";
const char* CaseMap::Locale::lithuanian_ = "lt";

CaseMap::Locale::Locale(const AtomicString& locale) {
  // Use the more optimized code path most of the time.
  //
  // Only Turkic (tr and az) languages and Lithuanian requires
  // locale-specific lowercasing rules. Even though CLDR has el-Lower,
  // it's identical to the locale-agnostic lowercasing. Context-dependent
  // handling of Greek capital sigma is built into the common lowercasing
  // function in ICU.
  //
  // Only Turkic (tr and az) languages, Greek and Lithuanian require
  // locale-specific uppercasing rules.
  if (UNLIKELY(LocaleIdMatchesLang(locale, "tr") ||
               LocaleIdMatchesLang(locale, "az")))
    case_map_locale_ = turkic_or_azeri_;
  else if (UNLIKELY(LocaleIdMatchesLang(locale, "el")))
    case_map_locale_ = greek_;
  else if (UNLIKELY(LocaleIdMatchesLang(locale, "lt")))
    case_map_locale_ = lithuanian_;
  else
    case_map_locale_ = nullptr;
}

scoped_refptr<StringImpl> CaseMap::ToLower(StringImpl* source) const {
  if (!case_map_locale_)
    return source->LowerUnicode();

  return CaseConvert(u_strToLower, source, case_map_locale_);
}

scoped_refptr<StringImpl> CaseMap::ToUpper(StringImpl* source) const {
  if (!case_map_locale_)
    return source->UpperUnicode();

  return CaseConvert(u_strToUpper, source, case_map_locale_);
}

String CaseMap::ToLower(const String& source) const {
  if (StringImpl* impl = source.Impl())
    return ToLower(impl);
  return String();
}

String CaseMap::ToUpper(const String& source) const {
  if (StringImpl* impl = source.Impl())
    return ToUpper(impl);
  return String();
}

UChar32 CaseMap::ToUpper(UChar32 c) const {
  if (UNLIKELY(case_map_locale_)) {
    if (case_map_locale_ == Locale::turkic_or_azeri_) {
      if (c == 'i')
        return kLatinCapitalLetterIWithDotAbove;
      if (c == kLatinSmallLetterDotlessI)
        return 'I';
    } else if (case_map_locale_ == Locale::lithuanian_) {
      // TODO(rob.buis) implement upper-casing rules for lt
      // like in StringImpl::upper(locale).
    }
  }

  return unicode::ToUpper(c);
}

}  // namespace WTF
