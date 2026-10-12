#pragma once

#include <string>

namespace crisp::ui {

// The HarmonyOS Sans SC fonts that the build copies from the checkout's fonts/ folder next to
// the executable (docs/ENVIRONMENT.md, section 4.3). The owner provides that folder and it is
// not in Git, so a build without it uses EUI-NEO's default font. Each member is a path relative
// to the executable's folder, as EUI-NEO resolves font files, or empty when the file is absent.
struct UiFonts {
    std::string text;    // body text and components (Regular)
    std::string medium;  // section headings (Medium)
    std::string title;   // page titles (Bold)
};

// The fonts present next to the running executable.
UiFonts FindUiFonts();

}  // namespace crisp::ui
