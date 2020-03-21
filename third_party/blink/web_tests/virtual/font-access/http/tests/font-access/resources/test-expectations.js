'use strict';

const MAC_FONTS = [
  {postscriptName: 'Monaco', fullName: 'Monaco', family: 'Monaco'},
  {postscriptName: 'Menlo-Regular', fullName: 'Menlo Regular', family: 'Menlo'},
  {postscriptName: 'Menlo-Bold', fullName: 'Menlo Bold', family: 'Menlo'},
  {postscriptName: 'Menlo-BoldItalic', fullName: 'Menlo Bold Italic', family: 'Menlo'},
  // Indic.
  {postscriptName: 'GujaratiMT', fullName: 'Gujarati MT', family: 'Gujarati MT'},
  {postscriptName: 'GujaratiMT-Bold', fullName: 'Gujarati MT Bold', family: 'Gujarati MT'},
  {postscriptName: 'DevanagariMT', fullName: 'Devanagari MT', family: 'Devanagari MT'},
  {postscriptName: 'DevanagariMT-Bold', fullName: 'Devanagari MT Bold', family: 'Devanagari MT'},
  // Japanese.
  {postscriptName: 'HiraMinProN-W3', fullName: 'Hiragino Mincho ProN W3', family: 'Hiragino Mincho ProN'},
  {postscriptName: 'HiraMinProN-W6', fullName: 'Hiragino Mincho ProN W6', family: 'Hiragino Mincho ProN'},
  // Korean.
  {postscriptName: 'AppleGothic', fullName: 'AppleGothic Regular', family: 'AppleGothic'},
  {postscriptName: 'AppleMyungjo', fullName: 'AppleMyungjo Regular', family: 'AppleMyungjo'},
  // Chinese.
  {postscriptName: 'STHeitiTC-Light', fullName: 'Heiti TC Light', family: 'Heiti TC'},
  {postscriptName: 'STHeitiTC-Medium', fullName: 'Heiti TC Medium', family: 'Heiti TC'},
];

function getExpectedFontSet() {
  // Verify (by font family) that some standard fonts have been returned.
  let platform;
  if (navigator.appVersion.indexOf("Win") !== -1) {
    platform = 'win';
  } else if (navigator.appVersion.indexOf("Mac") !== -1) {
    platform = 'mac';
  } else if (navigator.appVersion.indexOf("Linux") !== -1) {
    platform = 'linux';
  } else {
    platform = 'generic';
  }
  assert_true(platform !== 'generic', 'Platform must be detected.');
  if (platform === 'mac') {
    return MAC_FONTS;
  }

  return [];
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

  assert_true(postscriptNameSet.size == 0, `Postscript name set should be empty. Got: ${setToString(postscriptNameSet)}`);
  assert_true(fullNameSet.size == 0, `Full name set should be empty. Got: ${setToString(fullNameSet)}`);
  assert_true(familySet.size == 0, `Family set should be empty. Got: ${setToString(familySet)}`);
}

function setToString(set) {
  const items = Array.from(set);
  return JSON.stringify(items);
}
