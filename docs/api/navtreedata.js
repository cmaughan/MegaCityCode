/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Spectre", "index.html", [
    [ "Features", "index.html#autotoc_md3", null ],
    [ "Requirements", "index.html#autotoc_md4", [
      [ "Windows", "index.html#autotoc_md1", null ],
      [ "macOS", "index.html#autotoc_md2", null ],
      [ "Windows", "index.html#autotoc_md5", null ],
      [ "macOS", "index.html#autotoc_md6", null ]
    ] ],
    [ "Building", "index.html#autotoc_md7", [
      [ "Windows", "index.html#autotoc_md8", null ],
      [ "macOS", "index.html#autotoc_md9", null ]
    ] ],
    [ "Running", "index.html#autotoc_md10", [
      [ "Windows", "index.html#autotoc_md11", null ],
      [ "macOS", "index.html#autotoc_md12", null ]
    ] ],
    [ "Convenience Scripts", "index.html#autotoc_md13", null ],
    [ "Testing", "index.html#autotoc_md14", [
      [ "Windows", "index.html#autotoc_md15", null ],
      [ "macOS", "index.html#autotoc_md16", null ]
    ] ],
    [ "Render Snapshots", "index.html#autotoc_md17", null ],
    [ "Logging", "index.html#autotoc_md18", null ],
    [ "Project Layout", "index.html#autotoc_md19", null ],
    [ "CI", "index.html#autotoc_md20", null ],
    [ "Notes", "index.html#autotoc_md21", null ],
    [ "Architecture Diagrams", "index.html#autotoc_md22", [
      [ "CMake Target Dependencies", "index.html#autotoc_md23", null ],
      [ "Class Diagram", "index.html#autotoc_md24", null ],
      [ "Local API Docs", "index.html#autotoc_md25", null ]
    ] ],
    [ "Unicode Snapshot Example", "index.html#autotoc_md26", null ],
    [ "Module Map", "md_docs_module_map.html", [
      [ "Start Here", "md_docs_module_map.html#autotoc_md28", null ],
      [ "Main Libraries", "md_docs_module_map.html#autotoc_md29", [
        [ "<tt>app/</tt>", "md_docs_module_map.html#autotoc_md30", null ],
        [ "<tt>libs/spectre-types/</tt>", "md_docs_module_map.html#autotoc_md31", null ],
        [ "<tt>libs/spectre-window/</tt>", "md_docs_module_map.html#autotoc_md32", null ],
        [ "<tt>libs/spectre-renderer/</tt>", "md_docs_module_map.html#autotoc_md33", null ],
        [ "<tt>libs/spectre-font/</tt>", "md_docs_module_map.html#autotoc_md34", null ],
        [ "<tt>libs/spectre-grid/</tt>", "md_docs_module_map.html#autotoc_md35", null ],
        [ "<tt>libs/spectre-nvim/</tt>", "md_docs_module_map.html#autotoc_md36", null ]
      ] ],
      [ "Generated Views", "md_docs_module_map.html#autotoc_md37", [
        [ "Target graph", "md_docs_module_map.html#autotoc_md38", null ],
        [ "Class diagram", "md_docs_module_map.html#autotoc_md39", null ],
        [ "Local API docs", "md_docs_module_map.html#autotoc_md40", null ]
      ] ],
      [ "Validation Map", "md_docs_module_map.html#autotoc_md41", [
        [ "Fast confidence", "md_docs_module_map.html#autotoc_md42", null ],
        [ "Deterministic UI confidence", "md_docs_module_map.html#autotoc_md43", null ],
        [ "Documentation / hero image", "md_docs_module_map.html#autotoc_md44", null ]
      ] ],
      [ "Planning Links", "md_docs_module_map.html#autotoc_md45", null ],
      [ "Practical Heuristics", "md_docs_module_map.html#autotoc_md46", null ],
      [ "Why This Exists", "md_docs_module_map.html#autotoc_md47", null ]
    ] ],
    [ "Learnings", "md_docs_learnings.html", [
      [ "Tooling", "md_docs_learnings.html#autotoc_md50", [
        [ "Claude can generate and manipulate code dependency graphs", "md_docs_learnings.html#autotoc_md51", null ],
        [ "clang-uml for class-level dependency diagrams", "md_docs_learnings.html#autotoc_md53", null ],
        [ "Visual regression tests should capture renderer output, not the desktop", "md_docs_learnings.html#autotoc_md55", null ],
        [ "Hero screenshots need a separate path from deterministic render tests", "md_docs_learnings.html#autotoc_md57", null ],
        [ "Multi-line array parsing in render scenarios was a real bug", "md_docs_learnings.html#autotoc_md59", null ],
        [ "Plugin-driven screenshot actions should call APIs directly, not replay mappings", "md_docs_learnings.html#autotoc_md61", null ],
        [ "Python downsampling was fun, but the native high-resolution image won", "md_docs_learnings.html#autotoc_md63", null ],
        [ "A root shortcut script helps once the scripted workflows start to sprawl", "md_docs_learnings.html#autotoc_md65", null ],
        [ "Human-facing structure tools are worth the effort", "md_docs_learnings.html#autotoc_md67", null ]
      ] ],
      [ "Fonts, Emoji, and Fallbacks", "md_docs_learnings.html#autotoc_md69", [
        [ "Windows color emoji needs an end-to-end color glyph path", "md_docs_learnings.html#autotoc_md70", null ],
        [ "CJK tofu on Windows was mostly a fallback-font coverage problem", "md_docs_learnings.html#autotoc_md72", null ],
        [ "Bundled fonts and installed fonts can diverge in important ways", "md_docs_learnings.html#autotoc_md74", null ]
      ] ]
    ] ],
    [ "Work Items", "md_plans_work_items_index.html", null ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", null ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"classspectre_1_1_metal_renderer.html#adb4cab3393956bbd38431461b315b658",
"classspectre_1_1_vk_context.html#a476a9cb9bb1af7130307fee651646106",
"log_8h.html#aa16f711495209c388c02c17c51a9eaf7aa603905470e2a5b8c13e96b579ef0dba",
"structspectre_1_1_cursor_style.html",
"structspectre_1_1_render_test_scenario.html#a304a22df530fa0576e75809c467f52e7"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';