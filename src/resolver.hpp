#pragma once
#include "manifest.hpp"
#include <string>

void resolveMods(std::vector<ModFile> &files, const std::string &mcVersion,
                 const std::string &modLoader);