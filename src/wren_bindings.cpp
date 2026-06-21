#include "wren_bindings.hpp"
#include "wren.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

// method exposure

// Build.run("cmd") -> bool
static void build_run(WrenVM *vm) {
  const char *cmd = wrenGetSlotString(vm, 1);
  std::cout << "$ " << cmd << "\n";
  int ret = std::system(cmd);
  wrenSetSlotBool(vm, 0, ret == 0);
}

// Build.log("msg")
static void build_log(WrenVM *vm) {
  const char *msg = wrenGetSlotString(vm, 1);
  std::cout << "[build] " << msg << "\n";
}

// binding table
WrenForeignMethodFn lookupBinding(const char *module, const char *className,
                                  bool isStatic, const char *signature) {
  if (std::string(className) == "Build" && isStatic) {
    if (std::string(signature) == "run(_)")
      return build_run;
    if (std::string(signature) == "log(_)")
      return build_log;
  }
  return nullptr;
}

// VM read helpers
std::string getString(WrenVM *vm, WrenHandle *handle, const char *getter) {
  wrenEnsureSlots(vm, 1);
  WrenHandle *h = wrenMakeCallHandle(vm, getter);
  wrenSetSlotHandle(vm, 0, handle);
  wrenCall(vm, h);
  wrenReleaseHandle(vm, h);
  if (wrenGetSlotType(vm, 0) == WREN_TYPE_NULL)
    return "";
  return wrenGetSlotString(vm, 0);
}

// mod list reader
std::vector<ModFile> resolveModFiles(WrenVM *vm, WrenHandle *modpackHandle) {
  std::vector<ModFile> files;

  // Get modpack.mods list
  wrenEnsureSlots(vm, 2);
  WrenHandle *modsGetter = wrenMakeCallHandle(vm, "mods");
  wrenSetSlotHandle(vm, 0, modpackHandle);
  wrenCall(vm, modsGetter);
  wrenReleaseHandle(vm, modsGetter);
  WrenHandle *listHandle = wrenGetSlotHandle(vm, 0);

  // Get list length
  WrenHandle *countGetter = wrenMakeCallHandle(vm, "count");
  wrenSetSlotHandle(vm, 0, listHandle);
  wrenCall(vm, countGetter);
  int count = (int)wrenGetSlotDouble(vm, 0);
  wrenReleaseHandle(vm, countGetter);

  WrenHandle *subscript = wrenMakeCallHandle(vm, "[_]");

  for (int i = 0; i < count; i++) {
    wrenEnsureSlots(vm, 2);
    wrenSetSlotHandle(vm, 0, listHandle);
    wrenSetSlotDouble(vm, 1, (double)i);
    wrenCall(vm, subscript);

    WrenHandle *modHandle = wrenGetSlotHandle(vm, 0);

    auto get = [&](const char *g) -> std::string {
      return getString(vm, modHandle, g);
    };

    std::string side = get("side"); // "client", "server", "both"
    std::string source = get("source"); // "modrinth", "curseforge", "url"
    std::string slug = get("slug");

    ModFile file;
    file.path = "mods/" + slug + ".jar";
    file.env_client = (side == "server") ? "unsupported" : "required";
    file.env_server = (side == "client") ? "unsupported" : "required";

    if (source == "url") {
      file.downloadUrl = get("url");
    } else {
      file.path = "mods/" + slug + ".jar";
    }

    file.sha512 = slug;
    file.sha256 = source;

    files.push_back(file);
    wrenReleaseHandle(vm, modHandle);
  }

  wrenReleaseHandle(vm, subscript);
  wrenReleaseHandle(vm, listHandle);
  return files;
}