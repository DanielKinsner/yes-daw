// YES DAW - G1.1: writes docs/keymap-v2.md from the action descriptors.
//   YesDawKeymapDoc --out <path>     (prints the markdown to stdout without --out)
#include "ui/UiActions.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

int main (int argc, char** argv)
{
    std::string outPath;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp (argv[i], "--out") == 0)
            outPath = argv[i + 1];

    const yesdaw::ui::UiActionRegistry registry;
    const std::string markdown = registry.keymap().renderMarkdown();
    if (outPath.empty())
    {
        std::cout << markdown;
        return 0;
    }
    std::ofstream out (outPath, std::ios::binary);
    if (! out)
    {
        std::cerr << "cannot write " << outPath << "\n";
        return 1;
    }
    out << markdown;
    std::cout << "wrote " << outPath << " (" << markdown.size() << " bytes)\n";
    return 0;
}
