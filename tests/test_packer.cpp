#include "doctest/doctest.h"

#include <yyjson.h>
#include <zip.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "manifest.hpp"
#include "packer.hpp"

namespace fs = std::filesystem;

TEST_CASE("packMrpack creates valid mrpack zip without overrides") {
  Manifest m;
  m.name = "Test Mrpack";
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

  fs::path outPath =
      fs::temp_directory_path() / "please-speed-test-no-overrides.mrpack";
  fs::remove(outPath);

  packMrpack(m, "/nonexistent-overrides", outPath.string());

  // open the zip and inspect entries
  int err = 0;
  zip_t *archive = zip_open(outPath.string().c_str(), ZIP_RDONLY, &err);
  REQUIRE(archive != nullptr);

  CHECK_EQ(zip_get_num_entries(archive, 0), 1);

  zip_stat_t stat;
  zip_stat_init(&stat);
  CHECK_EQ(zip_stat(archive, "modrinth.index.json", 0, &stat), 0);
  CHECK_GT(stat.size, 0);

  // read and verify the index json
  zip_file_t *zf = zip_fopen(archive, "modrinth.index.json", 0);
  REQUIRE(zf != nullptr);

  std::vector<char> buf(stat.size + 1);
  CHECK_EQ(zip_fread(zf, buf.data(), stat.size), stat.size);
  buf[stat.size] = '\0';
  zip_fclose(zf);
  zip_close(archive);

  yyjson_doc *doc = yyjson_read(buf.data(), stat.size, 0);
  REQUIRE(doc != nullptr);

  yyjson_val *root = yyjson_doc_get_root(doc);
  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(root, "name"))),
           "Test Mrpack");
  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(root, "formatVersion"))),
           "1");

  yyjson_val *deps = yyjson_obj_get(root, "dependencies");
  CHECK(deps != nullptr);
  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(deps, "minecraft"))),
           "1.20.1");
  CHECK_EQ(std::string(yyjson_get_str(yyjson_obj_get(deps, "fabric-loader"))),
           "0.16.10");

  yyjson_val *files = yyjson_obj_get(root, "files");
  REQUIRE(files != nullptr);
  CHECK_EQ(yyjson_arr_size(files), 1);

  yyjson_doc_free(doc);

  fs::remove(outPath);
}

TEST_CASE("packMrpack includes overrides directory") {
  Manifest m;
  m.name = "Overrides Test";
  m.versionId = "1.0.0";
  m.mcVersion = "1.21";
  m.modLoader = "fabric";
  m.modLoaderVersion = "0.16.10";

  // setup overrides directory
  fs::path overridesDir =
      fs::temp_directory_path() / "please-speed-test-overrides";
  fs::remove_all(overridesDir);
  fs::create_directories(overridesDir / "config");

  std::ofstream cfg(overridesDir / "config" / "test.toml");
  cfg << "key = \"value\"\n";
  cfg.close();

  fs::path outPath =
      fs::temp_directory_path() / "please-speed-test-overrides.mrpack";
  fs::remove(outPath);

  packMrpack(m, overridesDir.string(), outPath.string());

  int err = 0;
  zip_t *archive = zip_open(outPath.string().c_str(), ZIP_RDONLY, &err);
  REQUIRE(archive != nullptr);

  CHECK_EQ(zip_get_num_entries(archive, 0), 2);

  zip_stat_t st;
  zip_stat_init(&st);
  CHECK_EQ(zip_stat(archive, "modrinth.index.json", 0, &st), 0);
  zip_stat_init(&st);
  CHECK_EQ(zip_stat(archive, "overrides/config/test.toml", 0, &st), 0);

  zip_close(archive);
  fs::remove(outPath);
  fs::remove_all(overridesDir);
}

TEST_CASE("packMultiMC creates valid MultiMC zip with overrides") {
  Manifest m;
  m.name = "MMC Test";
  m.versionId = "1.0.0";
  m.mcVersion = "1.20.1";
  m.modLoader = "forge";
  m.modLoaderVersion = "47.3.0";

  fs::path overridesDir =
      fs::temp_directory_path() / "please-speed-test-mmc-overrides";
  fs::remove_all(overridesDir);
  fs::create_directories(overridesDir);
  std::ofstream opt(overridesDir / "options.txt");
  opt << "renderDistance:12\n";
  opt.close();

  fs::path outPath = fs::temp_directory_path() / "please-speed-test-mmc.zip";
  fs::remove(outPath);

  packMultiMC(m, overridesDir.string(), outPath.string());

  int err = 0;
  zip_t *archive = zip_open(outPath.string().c_str(), ZIP_RDONLY, &err);
  REQUIRE(archive != nullptr);

  // should have at least: instance.cfg, mmc-pack.json, .minecraft/options.txt
  CHECK_GE(zip_get_num_entries(archive, 0), 3);

  zip_stat_t stat;
  zip_stat_init(&stat);
  CHECK_EQ(zip_stat(archive, "instance.cfg", 0, &stat), 0);

  zip_stat_init(&stat);
  CHECK_EQ(zip_stat(archive, "mmc-pack.json", 0, &stat), 0);

  zip_stat_init(&stat);
  CHECK_EQ(zip_stat(archive, ".minecraft/options.txt", 0, &stat), 0);

  // read and verify mmc-pack.json
  zip_file_t *zf = zip_fopen(archive, "mmc-pack.json", 0);
  REQUIRE(zf != nullptr);

  zip_stat_init(&stat);
  zip_stat(archive, "mmc-pack.json", 0, &stat);
  std::vector<char> buf(stat.size + 1);
  CHECK_EQ(zip_fread(zf, buf.data(), stat.size), stat.size);
  buf[stat.size] = '\0';
  zip_fclose(zf);

  yyjson_doc *doc = yyjson_read(buf.data(), stat.size, 0);
  REQUIRE(doc != nullptr);

  yyjson_val *root = yyjson_doc_get_root(doc);
  CHECK_EQ(yyjson_get_int(yyjson_obj_get(root, "formatVersion")), 1);

  yyjson_val *components = yyjson_obj_get(root, "components");
  REQUIRE(components != nullptr);
  CHECK_EQ(yyjson_arr_size(components), 2);

  yyjson_doc_free(doc);
  zip_close(archive);

  fs::remove(outPath);
  fs::remove_all(overridesDir);
}

TEST_CASE("packMultiMC includes mod files from cache") {
  Manifest m;
  m.name = "MMC Mods Test";
  m.versionId = "1.0.0";
  m.mcVersion = "1.20.1";
  m.modLoader = "fabric";
  m.modLoaderVersion = "0.16.10";

  ModFile f;
  f.path = "mods/test-cached-mod.jar";
  f.downloadUrl = "https://example.com/cached.jar";
  f.sha512 = std::string(128, 'a');
  f.sha256 = std::string(64, 'b');
  f.fileSize = 999;
  f.env_client = "required";
  f.env_server = "required";
  m.files.push_back(f);

  // setup cache directory in CWD
  fs::path cacheDir = fs::current_path() / ".please-speed-cache";
  fs::create_directories(cacheDir);
  std::ofstream cachedJar(cacheDir / "test-cached-mod.jar");
  cachedJar << "fake jar content";
  cachedJar.close();

  fs::path outPath =
      fs::temp_directory_path() / "please-speed-test-mmc-mods.zip";
  fs::remove(outPath);

  packMultiMC(m, "/nonexistent", outPath.string());

  int err = 0;
  zip_t *archive = zip_open(outPath.string().c_str(), ZIP_RDONLY, &err);
  REQUIRE(archive != nullptr);

  CHECK_EQ(zip_get_num_entries(archive, 0), 3);

  zip_stat_t stat;
  zip_stat_init(&stat);
  CHECK_EQ(zip_stat(archive, "instance.cfg", 0, &stat), 0);
  CHECK_EQ(zip_stat(archive, "mmc-pack.json", 0, &stat), 0);
  CHECK_EQ(zip_stat(archive, ".minecraft/mods/test-cached-mod.jar", 0, &stat),
           0);
  CHECK_EQ(stat.size, 16); // "fake jar content"

  zip_close(archive);

  fs::remove(outPath);
  fs::remove_all(cacheDir);
}
