#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "manifest.hpp"
#include "server_export.hpp"
#include <yyjson.h>

namespace fs = std::filesystem;

// helpers

static fs::path tempFile() {
  static int n = 0;
  return fs::temp_directory_path() / ("please-speed-test-" + std::to_string(++n) + ".tmp");
}

// hashFile

TEST_CASE("hashFile computes correct SHA-256 and SHA-512") {
  fs::path tmp = tempFile();
  {
    std::ofstream f(tmp);
    f << "hello world\n";
  }

  auto [sha512, sha256] = hashFile(tmp.string());
  fs::remove(tmp);

  CHECK_EQ(sha256, "a948904f2f0f479b8f8197694b30184b0d2ed1c1cd2a1ec0fb85d299a192a447");
  CHECK_EQ(sha512, "db3974a97f2407b7cae1ae637c0030687a11913274d578492558e39c16c017de84eacdc8c62fe34ee4e12b4b1428817f09b6a2760c3f8a664ceae94d2434a593");
}

TEST_CASE("hashFile throws on nonexistent file") {
  CHECK_THROWS_AS(hashFile("/nonexistent/path"), std::runtime_error);
}

// writeManifest

TEST_CASE("writeManifest produces valid JSON with correct fields") {
  Manifest m;
  m.name = "Test Pack";
  m.versionId = "1.0.0";
  m.mcVersion = "1.20.1";
  m.modLoader = "fabric";
  m.modLoaderVersion = "0.15.0";

  ModFile f;
  f.path = "mods/test-mod.jar";
  f.downloadUrl = "https://example.com/test.jar";
  f.sha512 = std::string(128, 'a');
  f.sha256 = std::string(64, 'b');
  f.fileSize = 12345;
  f.env_client = "required";
  f.env_server = "required";
  m.files.push_back(f);

  fs::path tmp = tempFile();
  writeManifest(m, tmp.string());

  std::ifstream in(tmp);
  std::string json((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
  fs::remove(tmp);

  yyjson_doc *doc = yyjson_read(json.c_str(), json.size(), 0);
  REQUIRE(doc != nullptr);

  yyjson_val *root = yyjson_doc_get_root(doc);

  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(root, "name"))), "Test Pack");
  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(root, "versionId"))), "1.0.0");
  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(root, "game"))), "minecraft");

  yyjson_val *deps = yyjson_obj_get(root, "dependencies");
  CHECK(deps != nullptr);
  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(deps, "minecraft"))), "1.20.1");

  yyjson_val *files = yyjson_obj_get(root, "files");
  REQUIRE(files != nullptr);
  CHECK_EQ(yyjson_arr_size(files), 1);

  yyjson_val *entry = yyjson_arr_get_first(files);
  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(entry, "path"))), "mods/test-mod.jar");

  yyjson_doc_free(doc);
}

// getJarServerFileName

TEST_CASE("getJarServerFileName returns correct jar names") {
  Manifest forge;
  forge.modLoader = "forge";
  CHECK_EQ(getJarServerFileName(forge), "forge.jar");

  Manifest fabric;
  fabric.modLoader = "fabric";
  CHECK_EQ(getJarServerFileName(fabric), "fabric-server-launch.jar");

  Manifest quilt;
  quilt.modLoader = "quilt";
  CHECK_EQ(getJarServerFileName(quilt), "quilt-server-launch.jar");

  Manifest unknown;
  unknown.modLoader = "unknown";
  CHECK_EQ(getJarServerFileName(unknown), "server.jar");
}

// manifest struct 

TEST_CASE("Manifest default values") {
  Manifest m;
  CHECK_EQ(m.formatVersion, "1");
  CHECK_EQ(m.game, "minecraft");
  CHECK(m.name.empty());
  CHECK(m.files.empty());
}

TEST_CASE("ModFile env values") {
  ModFile f;
  f.env_client = "required";
  f.env_server = "unsupported";
  CHECK_EQ(f.env_client, "required");
  CHECK_EQ(f.env_server, "unsupported");
}
