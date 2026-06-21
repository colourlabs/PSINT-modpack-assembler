#include <fstream>
#include <iostream>
#include <string>

#include "manifest.hpp"
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
    std::cerr << "error: cannot open " << path << "\n";
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
  std::cout << "=> reading build.wren\n";

  WrenVM *vm = makeVM();
  if (!runScript(vm, "build.wren")) {
    wrenFreeVM(vm);
    return 1;
  }

  Manifest m = readManifest(vm);
  wrenFreeVM(vm);

  writeManifest(m, "manifest.json");
  return 0;
}

int cmdPackPrism() {
  std::cout << "=> packing for Prism Launcher\n";

  WrenVM *vm = makeVM();
  if (!runScript(vm, "build.wren")) {
    wrenFreeVM(vm);
    return 1;
  }

  Manifest m = readManifest(vm);
  wrenFreeVM(vm);

  writeManifest(m, "manifest.json");
  // TODO: zip manifest.json + overrides/ into <name>.mrpack
  std::cout << "=> pack complete (manifest.json written)\n";
  return 0;
}

int cmdPackModrinth() {
  std::cout << "=> packing for Modrinth\n";

  WrenVM *vm = makeVM();
  if (!runScript(vm, "build.wren")) {
    wrenFreeVM(vm);
    return 1;
  }

  Manifest m = readManifest(vm);
  wrenFreeVM(vm);

  writeManifest(m, "manifest.json");
  // TODO: produce .mrpack bundle
  std::cout << "=> pack complete (manifest.json written)\n";
  return 0;
}

int cmdPackServer() {
  std::cout << "=> building server export\n";

  WrenVM *vm = makeVM();
  if (!runScript(vm, "build.wren")) {
    wrenFreeVM(vm);
    return 1;
  }

  Manifest m = readManifest(vm);
  wrenFreeVM(vm);

  writeServerExport(m, "server-export");
  return 0;
}

// entry point

static void printHelp() {
  std::cout << "usage: please-speed <command> [target]\n\n";
  std::cout << "commands:\n";
  std::cout << "  build server          read build.wren, write manifest.json\n";
  std::cout << "  pack prismlauncher    export a Prism Launcher modpack\n";
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
      std::cerr << "error: 'build' requires a target\n";
      std::cerr << "  please-speed build server\n";
      return 1;
    }
    std::string target = argv[2];
    if (target == "server")
      return cmdBuildServer();
    std::cerr << "error: unknown build target '" << target << "'\n";
    return 1;
  }

  // pack <target>
  if (command == "pack") {
    if (argc < 3) {
      std::cerr << "error: 'pack' requires a target\n";
      std::cerr << "  please-speed pack prismlauncher\n";
      std::cerr << "  please-speed pack modrinth\n";
      std::cerr << "  please-speed pack server\n";
      return 1;
    }
    std::string target = argv[2];
    if (target == "prismlauncher")
      return cmdPackPrism();
    if (target == "modrinth")
      return cmdPackModrinth();
    if (target == "server")
      return cmdPackServer();
    std::cerr << "error: unknown pack target '" << target << "'\n";
    std::cerr << "  valid: prismlauncher, modrinth, server\n";
    return 1;
  }

  std::cerr << "error: unknown command '" << command << "'\n";
  std::cerr << "run 'please-speed --help' for usage\n";
  return 1;
}