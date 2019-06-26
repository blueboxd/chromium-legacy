# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates Polymer3 UI elements (using JS modules) from existing Polymer2
# elements (using HTML imports). This is useful for avoiding code duplication
# while Polymer2 to Polymer3 migration is in progress.
#
# Variables:
#   html_file:
#     The input Polymer2 HTML file to be processed.
#
#   js_file:
#     The input Polymer2 JS file to be processed, or the name of the output JS
#     file when no input JS file exists (see |html_type| below).
#
#   in_folder:
#     The folder where |html_file| and |js_file| (when it exists) reside.
#
#   out_folder:
#     The output folder for the generated Polymer JS file.
#
#   html_type:
#     Specifies the type of the |html_file| such that the script knows how to
#     process the |html_file|. Available values are:
#       dom-module: A file holding a <dom-module> for a UI element (this is
#                   the majority case). Note: having multiple <dom-module>s
#                   within a single HTML file is not currently supported
#       style-module: A file holding a shared style <dom-module>
#                     (no corresponding Polymer2 JS file exists)
#       custom-style: A file holding a <custom-style> (usually a *_vars_css.html
#                     file, no corresponding Polymer2 JS file exists)
#       iron-iconset: A file holding one or more <iron-iconset-svg> instances
#                     (no corresponding Polymer2 JS file exists)
#       v3-ready: A file holding HTML that is already written for Polymer3. A
#                 Polymer3 JS file already exists for such cases. In this mode
#                 HTML content is simply pasted within the JS file. This mode
#                 will be the only supported mode after migration finishes.
#
#   namespace_rewrites:
#     A list of string replacements for replacing global namespaced references
#     with explicitly imported dependencies in the generated JS module.
#     For example "cr.foo.Bar|Bar" will replace all occurrences of "cr.foo.Bar"
#     with "Bar".

import argparse
import os
import re
import sys

_CWD = os.getcwd()

# Rewrite rules for removing unnecessary namespaces for example "cr.ui.Foo", to
# "Foo" within a generated JS module. Populated from command line arguments.
_namespace_rewrites = {}


def _extract_dependencies(html_file):
  with open(html_file, 'r') as f:
    lines = f.readlines()
    deps = []
    for line in lines:
      match = re.search(r'\s*<link rel="import" href="(.*)"', line)
      if match:
        deps.append(match.group(1))
  return deps;


def _rewrite_dependency_path(dep):
  if re.match(r'chrome://resources/polymer/v1_0/', dep):
    dep = dep.replace(r'/v1_0/', '/v3_0/')
    dep = dep.replace(r'.html', '.js')

  if dep.endswith('/html/polymer.html'):
    dep = "chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js"

  if re.match(r'chrome://resources/html/', dep):
    dep = dep.replace(r'/resources/html/', 'resources/js/')

  dep = dep.replace(r'.html', '.m.js')

  return dep


def _construct_js_import(dep):
  if dep.endswith('polymer_bundled.min.js'):
    return 'import {%s, %s} from \'%s\';' % ('Polymer', 'html', dep)
  # TODO(dpapad): Figure out how to pass these from the command line, such that
  # users of this script can pass their own default imports.
  if dep.endswith('paper-ripple-behavior.js'):
    return 'import {%s} from \'%s\';' % ('PaperRippleBehavior', dep)
  if dep.endswith('focus_outline_manager.m.js'):
    return 'import {%s} from \'%s\';' % ('FocusOutlineManager', dep)
  else:
    return 'import \'%s\';' % dep


def _generate_js_imports(html_file):
  return map(_construct_js_import,
      map(_rewrite_dependency_path, _extract_dependencies(html_file)))


def _extract_dom_module_id(html_file):
  with open(html_file, 'r') as f:
    contents = f.read()
    match = re.search(r'\s*<dom-module id="(.*)"', contents)
    assert match
    return match.group(1)


def _extract_template(html_file, html_type):
  if html_type == 'v3-ready':
    with open(html_file, 'r') as f:
      return f.read()

  if html_type == 'dom-module':
    with open(html_file, 'r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<dom-module ', line):
          assert start_line == -1
          assert end_line == -1
          assert re.match(r'\s*<template', lines[i + 1])
          start_line = i + 2;
        if re.match(r'\s*</dom-module>', line):
          assert start_line != -1
          assert end_line == -1
          assert re.match(r'\s*</template>', lines[i - 2])
          assert re.match(r'\s*<script ', lines[i - 1])
          end_line = i - 3;
    return '\n' + ''.join(lines[start_line:end_line + 1])

  if html_type == 'style-module':
    with open(html_file, 'r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<dom-module ', line):
          assert start_line == -1
          assert end_line == -1
          assert re.match(r'\s*<template', lines[i + 1])
          start_line = i + 1;
        if re.match(r'\s*</dom-module>', line):
          assert start_line != -1
          assert end_line == -1
          assert re.match(r'\s*</template>', lines[i - 1])
          end_line = i - 1;
    return '\n' + ''.join(lines[start_line:end_line + 1])


  if html_type == 'iron-iconset':
    templates = []
    with open(html_file, 'r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<iron-iconset-svg ', line):
          assert start_line == -1
          assert end_line == -1
          start_line = i;
        if re.match(r'\s*</iron-iconset-svg>', line):
          assert start_line != -1
          assert end_line == -1
          end_line = i
          templates.append(''.join(lines[start_line:end_line + 1]))
          # Reset indices.
          start_line = -1
          end_line = -1
    return '\n' + ''.join(templates)


  assert html_type == 'custom-style'
  with open(html_file, 'r') as f:
    lines = f.readlines()
    start_line = -1
    end_line = -1
    for i, line in enumerate(lines):
      if re.match(r'\s*<custom-style>', line):
        assert start_line == -1
        assert end_line == -1
        start_line = i;
      if re.match(r'\s*</custom-style>', line):
        assert start_line != -1
        assert end_line == -1
        end_line = i;

  return '\n' + ''.join(lines[start_line:end_line + 1])


# Replace various global references with their non-namespaced version, for
# example "cr.ui.Foo" becomes "Foo".
def _rewrite_namespaces(string):
  for rewrite in _namespace_rewrites:
    string = string.replace(rewrite, _namespace_rewrites[rewrite])
  return string


def _process_v3_ready(js_file, html_file):
  # Extract HTML template and place in JS file.
  html_template = _extract_template(html_file, 'v3-ready')

  with open(js_file) as f:
    lines = f.readlines()

  HTML_TEMPLATE_REGEX = '{__html_template__}'
  for i, line in enumerate(lines):
    line = line.replace(HTML_TEMPLATE_REGEX, html_template)
    lines[i] = line

  out_filename = os.path.basename(js_file)
  return lines, out_filename


def _process_dom_module(js_file, html_file):
  html_template = _extract_template(html_file, 'dom-module')
  js_imports = _generate_js_imports(html_file)

  with open(js_file) as f:
    lines = f.readlines()

  for i, line in enumerate(lines):
    # Place the JS imports right before the opening "Polymer({" line.
    # Note: Currently assuming there is only one Polymer declaration per page,
    # and no other code precedes it.
    # Place the HTML content right after the opening "Polymer({" line.
    line = line.replace(
        r'Polymer({',
        '%s\nPolymer({\n  _template: html`%s`,' % (
            '\n'.join(js_imports) + '\n', html_template))
    line = _rewrite_namespaces(line)
    lines[i] = line

  # Use .m.js extension for the generated JS file, since both files need to be
  # served by a chrome:// URL side-by-side.
  out_filename = os.path.basename(js_file).replace('.js', '.m.js')
  return lines, out_filename

def _process_style_module(js_file, html_file):
  html_template = _extract_template(html_file, 'style-module')
  js_imports = _generate_js_imports(html_file)

  style_id = _extract_dom_module_id(html_file)

  js_template = \
"""%(js_imports)s
const styleElement = document.createElement('dom-module');
styleElement.innerHTML = `%(html_template)s`;
styleElement.register('%(style_id)s');""" % {
  'js_imports': '\n'.join(js_imports),
  'html_template': html_template,
  'style_id': style_id,
}

  out_filename = os.path.basename(js_file)
  return js_template, out_filename


def _process_custom_style(js_file, html_file):
  html_template = _extract_template(html_file, 'custom-style')
  js_imports = _generate_js_imports(html_file)

  js_template = \
"""%(js_imports)s
const $_documentContainer = document.createElement('template');
$_documentContainer.innerHTML = `%(html_template)s`;
document.head.appendChild($_documentContainer.content);""" % {
  'js_imports': '\n'.join(js_imports),
  'html_template': html_template,
}

  out_filename = os.path.basename(js_file)
  return js_template, out_filename

def _process_iron_iconset(js_file, html_file):
  html_template = _extract_template(html_file, 'iron-iconset')
  js_imports = _generate_js_imports(html_file)

  js_template = \
"""%(js_imports)s
const template = html`%(html_template)s`;
document.head.appendChild(template.content);
""" % {
  'js_imports': '\n'.join(js_imports),
  'html_template': html_template,
}

  out_filename = os.path.basename(js_file)
  return js_template, out_filename


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--js_file', required=True)
  parser.add_argument('--html_file', required=True)
  parser.add_argument('--namespace_rewrites', required=False, nargs="*")
  parser.add_argument(
      '--html_type', choices=['dom-module', 'style-module', 'custom-style',
      'iron-iconset', 'v3-ready'],
      required=True)
  args = parser.parse_args(argv)

  # Extract namespace rewrites from arguments.
  if args.namespace_rewrites:
    for r in args.namespace_rewrites:
      before, after = r.split('|')
      _namespace_rewrites[before] = after

  in_folder = os.path.normpath(os.path.join(_CWD, args.in_folder))
  out_folder = os.path.normpath(os.path.join(_CWD, args.out_folder))

  js_file = os.path.join(in_folder, args.js_file)
  html_file = os.path.join(in_folder, args.html_file)

  result = ()
  if args.html_type == 'dom-module':
    result = _process_dom_module(js_file, html_file)
  if args.html_type == 'style-module':
    result = _process_style_module(js_file, html_file)
  elif args.html_type == 'custom-style':
    result = _process_custom_style(js_file, html_file)
  elif args.html_type == 'iron-iconset':
    result = _process_iron_iconset(js_file, html_file)
  elif args.html_type == 'v3-ready':
    result = _process_v3_ready(js_file, html_file)

  # Reconstruct file.
  with open(os.path.join(out_folder, result[1]), 'w') as f:
    for l in result[0]:
      f.write(l)
  return


if __name__ == '__main__':
  main(sys.argv[1:])
