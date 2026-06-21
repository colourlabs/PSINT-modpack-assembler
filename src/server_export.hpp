#pragma once
#include "manifest.hpp"
#include <string>

void writeServerExport(const Manifest& manifest, const std::string& outDir);

std::string getJarServerFileName(const Manifest &m);