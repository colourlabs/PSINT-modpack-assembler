#include <fstream>
#include <iostream>
#include <string>

#include "log.hpp"
#include "manifest.hpp"
#include "packer.hpp"
#include "resolver.hpp"
#include "server_export.hpp"
#include "wren_bindings.hpp"
#include "wren_modules.hpp"

#include <cxxopts.hpp>
#include <wren.hpp>

// wren callbacks

static void writeFn(WrenVM *, const char *text) { std::cout << text; }

static void errorFn(WrenVM *, WrenErrorType type, const char *module, int line,
                    const char *message) {
  switch (type) {
  case WREN_ERROR_COMPILE:
    std::cerr << "[compile] " << module << ":" << line << " " << message
              << "\n";
    break;
  case WREN_ERROR_RUNTIME:
    std::cerr << "[runtime] " << message << "\n";
    break;
  case WREN_ERROR_STACK_TRACE:
    std::cerr << "  at " << module << ":" << line << " in " << message << "\n";
    break;
  }
}

static WrenLoadModuleResult loadModule(WrenVM *, const char *name) {
  WrenLoadModuleResult result = {0};

  if (std::string(name) == "please-speed") {
    result.source = PLEASE_SPEED_WREN;
    return result;
  }

  std::string path = std::string("modules/") + name + ".wren";
  std::ifstream f(path);
  if (!f)
    return result;

  std::string src((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
  result.source = copyString(src.c_str());
  return result;
}

static WrenForeignMethodFn bindForeignMethod(WrenVM *, const char *module,
                                             const char *className,
                                             bool isStatic,
                                             const char *signature) {
  return lookupBinding(module, className, isStatic, signature);
}

// VM lifecycle

struct WrenVMGuard {
  WrenVM *vm;
  explicit WrenVMGuard(WrenVM *v) : vm(v) {}
  ~WrenVMGuard() { if (vm) wrenFreeVM(vm); }
  WrenVMGuard(const WrenVMGuard &) = delete;
  WrenVMGuard &operator=(const WrenVMGuard &) = delete;
};

WrenVM *makeVM() {
  WrenConfiguration config;
  wrenInitConfiguration(&config);
  config.writeFn = writeFn;
  config.errorFn = errorFn;
  config.loadModuleFn = loadModule;
  config.bindForeignMethodFn = bindForeignMethod;
  return wrenNewVM(&config);
}

bool runScript(WrenVM *vm, const std::string &path) {
  std::ifstream f(path);
  if (!f) {
    logging::error("cannot open " + path);
    return false;
  }
  std::string src((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
  return wrenInterpret(vm, "main", src.c_str()) == WREN_RESULT_SUCCESS;
}

// read modpack config from VM

Manifest readManifest(WrenVM *vm) {
  wrenEnsureSlots(vm, 1);
  wrenGetVariable(vm, "main", "modpack", 0);
  WrenHandle *handle = wrenGetSlotHandle(vm, 0);

  Manifest m;
  m.name = getString(vm, handle, "name");
  m.versionId = getString(vm, handle, "version_id");
  m.mcVersion = getString(vm, handle, "mc_version");
  m.modLoader = getString(vm, handle, "mod_loader");
  m.modLoaderVersion = getString(vm, handle, "mod_loader_version");
  m.files = resolveModFiles(vm, handle);

  wrenReleaseHandle(vm, handle);
  resolveMods(m.files, m.mcVersion, m.modLoader);

  return m;
}

// commands

int cmdBuildServer() {
  logging::step("reading build.wren");

  WrenVMGuard guard(makeVM());
  if (!runScript(guard.vm, "build.wren"))
    return 1;

  Manifest m = readManifest(guard.vm);

  writeManifest(m, "manifest.json");
  return 0;
}

int cmdPackPrismMmc() {
  logging::step("packing for Prism Launcher / MultiMC");

  WrenVMGuard guard(makeVM());
  if (!runScript(guard.vm, "build.wren"))
    return 1;

  Manifest m = readManifest(guard.vm);

  std::string outPath = m.name + "-" + m.versionId + "-multimc.zip";
  packMultiMC(m, "overrides", outPath);

  return 0;
}

int cmdPackModrinth() {
  logging::step("packing for Modrinth");

  WrenVMGuard guard(makeVM());
  if (!runScript(guard.vm, "build.wren"))
    return 1;

  Manifest m = readManifest(guard.vm);

  std::string outPath = m.name + "-" + m.versionId + ".mrpack";
  packMrpack(m, "overrides", outPath);
  return 0;
}

int cmdPackServer() {
  logging::step("building server export");

  WrenVMGuard guard(makeVM());
  if (!runScript(guard.vm, "build.wren"))
    return 1;

  Manifest m = readManifest(guard.vm);

  writeServerExport(m, "server-export");
  return 0;
}

// entry point

static void printHelp() {
  std::cout << "usage: please-speed <command> [target]\n\n";
  std::cout << "commands:\n";
  std::cout << "  build server          read build.wren, write manifest.json\n";
  std::cout
      << "  pack prismlauncher    export a Prism Launcher / MultiMC modpack\n";
  std::cout << "  pack multimc          export a Prism Launcher modpack\n";
  std::cout << "  pack modrinth         export a Modrinth .mrpack\n";
  std::cout << "  pack server           generate install.sh + install.bat\n\n";
  std::cout << "options:\n";
  std::cout << "  -h, --help            show this help\n";
}

int main(int argc, char **argv) {
  cxxopts::Options options("please-speed", "Modpack build tool");
  options.allow_unrecognised_options();
  options.add_options()("h,help", "Show help");

  auto result = options.parse(argc, argv);

  if (result.count("help") || argc < 2) {
    printHelp();
    return 0;
  }

  std::string command = argv[1];

  // build <target>
  if (command == "build") {
    if (argc < 3) {
      logging::error("'build' requires a target");
      logging::info("please-speed build server");
      return 1;
    }
    std::string target = argv[2];
    if (target == "server")
      return cmdBuildServer();
    logging::error("unknown build target '" + target + "'");
    return 1;
  }

  // pack <target>
  if (command == "pack") {
    if (argc < 3) {
      logging::error("'pack' requires a target");
      logging::info("please-speed pack multimc");
      logging::info("please-speed pack prismlauncher");
      logging::info("please-speed pack modrinth");
      logging::info("please-speed pack server");
      return 1;
    }
    std::string target = argv[2];
    if (target == "prismlauncher")
      return cmdPackPrismMmc();
    if (target == "multimc")
      return cmdPackPrismMmc();
    if (target == "modrinth")
      return cmdPackModrinth();
    if (target == "server")
      return cmdPackServer();
    logging::error("unknown pack target '" + target + "'");
    logging::info("valid: prismlauncher, modrinth, server");
    return 1;
  }

  logging::error("unknown command '" + command + "'");
  logging::info("run 'please-speed --help' for usage");
  return 1;
}