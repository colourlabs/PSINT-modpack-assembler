#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "manifest.hpp"
#include "server_export.hpp"

namespace fs = std::filesystem;

TEST_CASE("writeServerExport creates install.sh and install.bat for fabric") {
  Manifest m;
  m.name = "Fabric Test";
  m.versionId = "1.0.0";
  m.mcVersion = "1.20.1";
  m.modLoader = "fabric";
  m.modLoaderVersion = "0.16.10";

  ModFile f;
  f.path = "mods/test-mod.jar";
  f.downloadUrl = "https://example.com/test.jar";
  f.sha512 = std::string(128, 'a');
  f.sha256 = std::string(64, 'b');
  f.fileSize = 12345;
  f.env_client = "required";
  f.env_server = "required";
  m.files.push_back(f);

  fs::path outDir = fs::temp_directory_path() / "please-speed-test-srv-export";
  fs::remove_all(outDir);

  writeServerExport(m, outDir.string());

  fs::path shellPath = outDir / "install.sh";
  CHECK(fs::exists(shellPath));
  auto perms = fs::status(shellPath).permissions();
  CHECK((perms & fs::perms::owner_exec) != fs::perms::none);

  std::ifstream shellIn(shellPath);
  std::string shell((std::istreambuf_iterator<char>(shellIn)),
                    std::istreambuf_iterator<char>());
  CHECK(shell.find("fabric-installer") != std::string::npos);
  CHECK(shell.find("test-mod.jar") != std::string::npos);
  CHECK(shell.find("fabric-server-launch.jar") != std::string::npos);

  fs::path batchPath = outDir / "install.bat";
  CHECK(fs::exists(batchPath));

  std::ifstream batchIn(batchPath);
  std::string batch((std::istreambuf_iterator<char>(batchIn)),
                    std::istreambuf_iterator<char>());
  CHECK(batch.find("fabric-installer") != std::string::npos);
  CHECK(batch.find("test-mod.jar") != std::string::npos);

  fs::remove_all(outDir);
}

TEST_CASE("writeServerExport excludes client-only mods, includes forge") {
  Manifest m;
  m.name = "Forge Test";
  m.versionId = "1.0.0";
  m.mcVersion = "1.20.1";
  m.modLoader = "forge";
  m.modLoaderVersion = "47.3.0";

  ModFile serverMod;
  serverMod.path = "mods/server-mod.jar";
  serverMod.downloadUrl = "https://example.com/server.jar";
  serverMod.env_client = "unsupported";
  serverMod.env_server = "required";
  m.files.push_back(serverMod);

  ModFile clientMod;
  clientMod.path = "mods/client-mod.jar";
  clientMod.downloadUrl = "https://example.com/client.jar";
  clientMod.env_client = "required";
  clientMod.env_server = "unsupported";
  m.files.push_back(clientMod);

  fs::path outDir = fs::temp_directory_path() / "please-speed-test-side";
  fs::remove_all(outDir);

  writeServerExport(m, outDir.string());

  fs::path shellPath = outDir / "install.sh";
  std::ifstream shellIn(shellPath);
  std::string shell((std::istreambuf_iterator<char>(shellIn)),
                    std::istreambuf_iterator<char>());

  CHECK(shell.find("server-mod.jar") != std::string::npos);
  CHECK(shell.find("client-mod.jar") == std::string::npos);
  CHECK(shell.find("installer.jar") != std::string::npos);
  CHECK(shell.find("forge.jar") != std::string::npos);

  fs::remove_all(outDir);
}

TEST_CASE("writeServerExport handles quilt loader") {
  Manifest m;
  m.name = "Quilt Test";
  m.versionId = "1.0.0";
  m.mcVersion = "1.20.1";
  m.modLoader = "quilt";
  m.modLoaderVersion = "0.25.0";

  fs::path outDir = fs::temp_directory_path() / "please-speed-test-quilt";
  fs::remove_all(outDir);

  writeServerExport(m, outDir.string());

  fs::path shellPath = outDir / "install.sh";
  std::ifstream shellIn(shellPath);
  std::string shell((std::istreambuf_iterator<char>(shellIn)),
                    std::istreambuf_iterator<char>());

  CHECK(shell.find("quiltmc.org") != std::string::npos);
  CHECK(shell.find("quilt-server-launch.jar") != std::string::npos);
  CHECK(shell.find("start.sh") != std::string::npos);

  fs::remove_all(outDir);
}
