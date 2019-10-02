// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_linux.h"

#include <fontconfig/fontconfig.h>

#include <map>
#include <memory>
#include <string>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/font.h"
#include "ui/gfx/linux/fontconfig_util.h"

namespace gfx {

namespace {

const char kFontFormatTrueType[] = "TrueType";
const char kFontFormatCFF[] = "CFF";

bool IsValidFontFromPattern(FcPattern* pattern) {
  // Ignore any bitmap fonts users may still have installed from last
  // century.
  if (!IsFontScalable(pattern))
    return false;

  // Take only supported font formats on board.
  std::string format = GetFontFormat(pattern);
  if (format != kFontFormatTrueType && format != kFontFormatCFF)
    return false;

  // Ignore any fonts FontConfig knows about, but that we don't have
  // permission to read.
  base::FilePath font_path = GetFontPath(pattern);
  if (font_path.empty() || access(font_path.AsUTF8Unsafe().c_str(), R_OK))
    return false;

  return true;
}

}  // namespace

FallbackFontsCache* GetFallbackFontsCacheInstance() {
  constexpr int kFallbackCacheSize = 64;
  static base::NoDestructor<FallbackFontsCache> fallback_cache(
      kFallbackCacheSize);
  return fallback_cache.get();
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     base::StringPiece16 text,
                     Font* result) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFont");

  // The text passed must be at least length 1.
  if (text.empty())
    return false;

  ScopedFcPattern pattern(FcPatternCreate());

  // Add pattern for family name.
  std::string font_family = font.GetFontName();
  FcPatternAddString(pattern.get(), FC_FAMILY,
                     reinterpret_cast<const FcChar8*>(font_family.c_str()));

  // Prefer scalable font.
  FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);

  // Add pattern for locale.
  FcPatternAddString(pattern.get(), FC_LANG,
                     reinterpret_cast<const FcChar8*>(locale.c_str()));

  // Add pattern for font style.
  if ((font.GetStyle() & gfx::Font::ITALIC) != 0)
    FcPatternAddInteger(pattern.get(), FC_SLANT, FC_SLANT_ITALIC);

  // Add pattern for characters.
  FcCharSet* charset = FcCharSetCreate();
  size_t offset = 0;
  while (offset < text.length()) {
    UChar32 code_point;
    U16_NEXT(text.data(), offset, text.length(), code_point);
    FcCharSetAddChar(charset, code_point);
  }
  FcPatternAddCharSet(pattern.get(), "charset", charset);
  FcCharSetDestroy(charset);

  // Match a font fallback.
  FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
  FcDefaultSubstitute(pattern.get());

  FcResult fc_result;
  ScopedFcPattern matched_pattern(
      FcFontMatch(nullptr, pattern.get(), &fc_result));
  if (!matched_pattern || !IsValidFontFromPattern(matched_pattern.get()))
    return false;

  FcChar8* fallback_family = nullptr;
  FcPatternGetString(matched_pattern.get(), FC_FAMILY, 0, &fallback_family);
  if (!fallback_family)
    return false;

  *result =
      Font(reinterpret_cast<const char*>(fallback_family), font.GetFontSize());
  return true;
}

std::vector<Font> GetFallbackFonts(const Font& font) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFonts");

  std::string font_family = font.GetFontName();

  // Lookup in the cache for already processed family.
  FallbackFontsCache* font_cache = GetFallbackFontsCacheInstance();
  auto cached_fallback_fonts = font_cache->Get(font_family);
  if (cached_fallback_fonts != font_cache->end()) {
    // Already in cache.
    return cached_fallback_fonts->second;
  }

  // Retrieve the font fallbacks for a given family name.
  std::vector<Font> fallback_fonts;
  FcPattern* pattern = FcPatternCreate();
  FcPatternAddString(pattern, FC_FAMILY,
                     reinterpret_cast<const FcChar8*>(font_family.c_str()));

  if (FcConfigSubstitute(nullptr, pattern, FcMatchPattern) == FcTrue) {
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcFontSet* fonts = FcFontSort(nullptr, pattern, FcTrue, nullptr, &result);
    if (fonts) {
      std::set<std::string> fallback_names;
      for (int i = 0; i < fonts->nfont; ++i) {
        std::string name_str = GetFontName(fonts->fonts[i]);
        if (name_str.empty())
          continue;

        // FontConfig returns multiple fonts with the same family name and
        // different configurations. Check to prevent duplicate family names.
        if (fallback_names.insert(name_str).second)
          fallback_fonts.push_back(Font(name_str, 13));
      }
      FcFontSetDestroy(fonts);
    }
  }
  FcPatternDestroy(pattern);

  // Store the font fallbacks to the cache.
  font_cache->Put(font_family, fallback_fonts);

  return fallback_fonts;
}

namespace {

class CachedFont {
 public:
  // Note: We pass the charset explicitly as callers
  // should not create CachedFont entries without knowing
  // that the FcPattern contains a valid charset.
  CachedFont(FcPattern* pattern, FcCharSet* char_set)
      : supported_characters_(char_set) {
    DCHECK(pattern);
    DCHECK(char_set);
    fallback_font_.name = GetFontName(pattern);
    fallback_font_.filename = GetFontPath(pattern).AsUTF8Unsafe();
    fallback_font_.ttc_index = GetFontTtcIndex(pattern);
    fallback_font_.is_bold = IsFontBold(pattern);
    fallback_font_.is_italic = IsFontItalic(pattern);
  }

  const FallbackFontData& fallback_font() const { return fallback_font_; }

  bool HasGlyphForCharacter(UChar32 c) const {
    return supported_characters_ && FcCharSetHasChar(supported_characters_, c);
  }

 private:
  FallbackFontData fallback_font_;
  // supported_characters_ is owned by the parent
  // FcFontSet and should never be freed.
  FcCharSet* supported_characters_;
};

class CachedFontSet {
 public:
  // CachedFontSet takes ownership of the passed FcFontSet.
  static std::unique_ptr<CachedFontSet> CreateForLocale(
      const std::string& locale) {
    FcFontSet* font_set = CreateFcFontSetForLocale(locale);
    return base::WrapUnique(new CachedFontSet(font_set));
  }

  ~CachedFontSet() {
    fallback_list_.clear();
    FcFontSetDestroy(font_set_);
  }

  FallbackFontData GetFallbackFontForChar(UChar32 c) {
    TRACE_EVENT0("fonts", "gfx::CachedFontSet::GetFallbackFontForChar");

    for (const auto& cached_font : fallback_list_) {
      if (cached_font.HasGlyphForCharacter(c))
        return cached_font.fallback_font();
    }
    // The previous code just returned garbage if the user didn't
    // have the necessary fonts, this seems better than garbage.
    // Current callers happen to ignore any values with an empty family string.
    return FallbackFontData();
  }

 private:
  static FcFontSet* CreateFcFontSetForLocale(const std::string& locale) {
    FcPattern* pattern = FcPatternCreate();

    if (!locale.empty()) {
      // FcChar* is unsigned char* so we have to cast.
      FcPatternAddString(pattern, FC_LANG,
                         reinterpret_cast<const FcChar8*>(locale.c_str()));
    }

    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    if (locale.empty())
      FcPatternDel(pattern, FC_LANG);

    // The result parameter returns if any fonts were found.
    // We already handle 0 fonts correctly, so we ignore the param.
    FcResult result;
    FcFontSet* font_set = FcFontSort(0, pattern, 0, 0, &result);
    FcPatternDestroy(pattern);

    // The caller will take ownership of this FcFontSet.
    return font_set;
  }

  CachedFontSet(FcFontSet* font_set) : font_set_(font_set) {
    FillFallbackList();
  }

  void FillFallbackList() {
    TRACE_EVENT0("fonts", "gfx::CachedFontSet::FillFallbackList");

    DCHECK(fallback_list_.empty());
    if (!font_set_)
      return;

    for (int i = 0; i < font_set_->nfont; ++i) {
      FcPattern* pattern = font_set_->fonts[i];

      if (!IsValidFontFromPattern(pattern))
        continue;

      // Make sure this font can tell us what characters it has glyphs for.
      FcCharSet* char_set;
      if (FcPatternGetCharSet(pattern, FC_CHARSET, 0, &char_set) !=
          FcResultMatch)
        continue;

      fallback_list_.emplace_back(pattern, char_set);
    }
  }

  FcFontSet* font_set_;  // Owned by this object.
  // CachedFont has a FcCharset* which points into the FcFontSet.
  // If the FcFontSet is ever destroyed, the fallback list
  // must be cleared first.
  std::vector<CachedFont> fallback_list_;

  DISALLOW_COPY_AND_ASSIGN(CachedFontSet);
};

typedef std::map<std::string, std::unique_ptr<CachedFontSet>> FontSetCache;
base::LazyInstance<FontSetCache>::Leaky g_font_sets_by_locale =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FallbackFontData GetFallbackFontForChar(UChar32 c, const std::string& locale) {
  auto& cached_font_set = g_font_sets_by_locale.Get()[locale];
  if (!cached_font_set)
    cached_font_set = CachedFontSet::CreateForLocale(locale);
  return cached_font_set->GetFallbackFontForChar(c);
}

}  // namespace gfx
