#pragma once
#include "manifest.hpp"
#include <string>

void packMrpack(const Manifest &m, const std::string &overridesDir,
                const std::string &outPath);

void packMultiMC(const Manifest &m, const std::string &overridesDir,
                 const std::string &outPath);