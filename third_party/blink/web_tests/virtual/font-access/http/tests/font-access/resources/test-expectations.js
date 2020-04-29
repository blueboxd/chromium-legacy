'use strict';

const MAC_FONTS = [
  {
    postscriptName: 'Monaco',
    fullName: 'Monaco',
    family: 'Monaco',
    expectedTables: [
      // Tables related to TrueType.
      'cvt ', 'glyf', 'loca', 'prep', 'gasp',
    ],
  },
  {
    postscriptName: 'Menlo-Regular',
    fullName: 'Menlo Regular',
    family: 'Menlo',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  {
    postscriptName: 'Menlo-Bold',
    fullName: 'Menlo Bold',
    family: 'Menlo',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  {
    postscriptName: 'Menlo-BoldItalic',
    fullName: 'Menlo Bold Italic',
    family: 'Menlo',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  // Indic.
  {
    postscriptName: 'GujaratiMT',
    fullName: 'Gujarati MT',
    family: 'Gujarati MT',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  {
    postscriptName: 'GujaratiMT-Bold',
    fullName: 'Gujarati MT Bold',
    family: 'Gujarati MT',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  {
    postscriptName: 'DevanagariMT',
    fullName: 'Devanagari MT',
    family: 'Devanagari MT',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  {
    postscriptName: 'DevanagariMT-Bold',
    fullName: 'Devanagari MT Bold',
    family: 'Devanagari MT',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  // Japanese.
  {
    postscriptName: 'HiraMinProN-W3',
    fullName: 'Hiragino Mincho ProN W3',
    family: 'Hiragino Mincho ProN',
    expectedTables: [
      'CFF ', 'VORG',
    ],
  },
  {
    postscriptName: 'HiraMinProN-W6',
    fullName: 'Hiragino Mincho ProN W6',
    family: 'Hiragino Mincho ProN',
    expectedTables: [
      'CFF ', 'VORG',
    ],
  },
  // Korean.
  {
    postscriptName: 'AppleGothic',
    fullName: 'AppleGothic Regular',
    family: 'AppleGothic',
    expectedTables: [
      'cvt ', 'glyf', 'loca',
    ],
  },
  {
    postscriptName: 'AppleMyungjo',
    fullName: 'AppleMyungjo Regular',
    family: 'AppleMyungjo',
    expectedTables: [
      'cvt ', 'glyf', 'loca',
    ],
  },
  // Chinese.
  {
    postscriptName: 'STHeitiTC-Light',
    fullName: 'Heiti TC Light',
    family: 'Heiti TC',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  {
   postscriptName: 'STHeitiTC-Medium',
    fullName: 'Heiti TC Medium',
    family: 'Heiti TC',
    expectedTables: [
      'cvt ', 'glyf', 'loca', 'prep',
    ],
  },
  // Bitmap.
  {
    postscriptName: 'AppleColorEmoji',
    fullName: 'Apple Color Emoji',
    family: 'Apple Color Emoji',
    expectedTables: [
      'glyf', 'loca',
      // Tables related to Bitmap Glyphs.
      'sbix',
    ],
  },
];

// The OpenType spec mentions that the follow tables are required for a font to
// function correctly. We'll have all the tables listed except for OS/2, which
// is not present in all fonts on Mac OS.
// https://docs.microsoft.com/en-us/typography/opentype/spec/otff#font-tables
const BASE_TABLES = [
  'cmap',
  'head',
  'hhea',
  'hmtx',
  'maxp',
  'name',
  'post',
];

function getExpectedFontSet() {
  // Verify (by font family) that some standard fonts have been returned.
  let platform;
  if (navigator.platform.indexOf("Win") !== -1) {
    platform = 'win';
  } else if (navigator.platform.indexOf("Mac") !== -1) {
    platform = 'mac';
  } else if (navigator.platform.indexOf("Linux") !== -1) {
    platform = 'linux';
  } else {
    platform = 'generic';
  }

  assert_not_equals(platform, 'generic', 'Platform must be detected.');
  if (platform === 'mac') {
    return MAC_FONTS;
  }

  return [];
}

function getMoreExpectedTables(expectations) {
  const output = {};
  for (const f of expectations) {
    if (f.expectedTables) {
      output[f.postscriptName] = f.expectedTables;
    }
  }
  return output;
}

async function filterFontSet(iterator, expectedFonts) {

  const nameSet = new Set();
  for (const e of expectedFonts) {
    nameSet.add(e.postscriptName);
  }

  const output = [];
  for await (const f of iterator) {
    if (nameSet.has(f.postscriptName)) {
      output.push(f);
    }
  }

  const numGot = output.length;
  const numExpected = Object.keys(expectedFonts).length;
  assert_equals(numGot, numExpected, `Got ${numGot} fonts, expected ${numExpected}.`);

  return output;
}

function assert_fonts_exist(availableFonts, expectedFonts) {
  const postscriptNameSet = new Set();
  const fullNameSet = new Set();
  const familySet = new Set();

  for (const f of expectedFonts) {
    postscriptNameSet.add(f.postscriptName);
    fullNameSet.add(f.fullName);
    familySet.add(f.family);
  }

  for (const f of availableFonts) {
    postscriptNameSet.delete(f.postscriptName);
    fullNameSet.delete(f.fullName);
    familySet.delete(f.family);
  }

  assert_equals(postscriptNameSet.size, 0,
              `Missing Postscript names: ${setToString(postscriptNameSet)}.`);
  assert_equals(fullNameSet.size, 0,
              `Missing Full names: ${setToString(fullNameSet)}.`);
  assert_equals(familySet.size, 0,
              `Missing Families: ${setToString(familySet)}.`);
}

function assert_font_has_tables(name, tables, expectedTables) {
  for (const t of expectedTables) {
    assert_equals(t.length, 4,
                "Table names are always 4 characters long.");
    assert_true(tables.has(t),
                `Font ${name} did not have required table ${t}.`);
    assert_greater_than(tables.get(t).size, 0,
                `Font ${name} has table ${t} of size 0.`);
  }
}

function setToString(set) {
  const items = Array.from(set);
  return JSON.stringify(items);
}
