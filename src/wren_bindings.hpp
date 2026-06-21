#pragma once
#include "manifest.hpp"
#include "wren.hpp"
#include <string>

WrenForeignMethodFn lookupBinding(const char *module, const char *className,
                                  bool isStatic, const char *signature);

std::string getString(WrenVM *vm, WrenHandle *handle, const char *getter);
std::vector<ModFile> resolveModFiles(WrenVM *vm, WrenHandle *modpackHandle);