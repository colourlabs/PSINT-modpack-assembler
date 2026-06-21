#include "packer.hpp"
#include "manifest.hpp"

#include <yyjson.h>
#include <zip.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

// zip helpers

static void zipAddFile(zip_t *archive, const std::string &diskPath,
                       const std::string &zipPath) {
  zip_source_t *src = zip_source_file(archive, diskPath.c_str(), 0, -1);
  if (!src)
    throw std::runtime_error("zip: cannot source file: " + diskPath);

  if (zip_file_add(archive, zipPath.c_str(), src, ZIP_FL_OVERWRITE) < 0) {
    zip_source_free(src);
    throw std::runtime_error("zip: cannot add file: " + zipPath);
  }
}

static void zipAddString(zip_t *archive, const std::string &contents,
                         const std::string &zipPath) {
  zip_source_t *src =
      zip_source_buffer(archive, contents.c_str(), contents.size(), 0);
  if (!src)
    throw std::runtime_error("zip: cannot create buffer source for: " +
                             zipPath);

  if (zip_file_add(archive, zipPath.c_str(), src, ZIP_FL_OVERWRITE) < 0) {
    zip_source_free(src);
    throw std::runtime_error("zip: cannot add string to zip: " + zipPath);
  }
}

static void zipAddDir(zip_t *archive, const fs::path &dirPath,
                      const std::string &zipPrefix) {
  for (const auto &entry : fs::recursive_directory_iterator(dirPath)) {
    if (!entry.is_regular_file())
      continue;

    fs::path rel = fs::relative(entry.path(), dirPath);
    std::string zipPath = zipPrefix + "/" + rel.string();

    std::replace(zipPath.begin(), zipPath.end(), '\\', '/');

    std::cout << "  adding override: " << zipPath << "\n";
    zipAddFile(archive, entry.path().string(), zipPath);
  }
}

// manifest.json builder (modrinth index format)

static std::string buildIndexJson(const Manifest &m) {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_obj_add_str(doc, root, "formatVersion", "1");
  yyjson_mut_obj_add_str(doc, root, "game", "minecraft");
  yyjson_mut_obj_add_str(doc, root, "name", m.name.c_str());
  yyjson_mut_obj_add_str(doc, root, "versionId", m.versionId.c_str());

  yyjson_mut_val *deps = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_str(doc, deps, "minecraft", m.mcVersion.c_str());
  std::string loaderKey = m.modLoader + "-loader";
  yyjson_mut_obj_add_str(doc, deps, loaderKey.c_str(),
                         m.modLoaderVersion.c_str());
  yyjson_mut_obj_add_val(doc, root, "dependencies", deps);

  yyjson_mut_val *files = yyjson_mut_arr(doc);

  for (const auto &file : m.files) {
    yyjson_mut_val *entry = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, entry, "path", file.path.c_str());
    yyjson_mut_obj_add_int(doc, entry, "fileSize", (int64_t)file.fileSize);

    yyjson_mut_val *hashes = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, hashes, "sha512", file.sha512.c_str());
    yyjson_mut_obj_add_str(doc, hashes, "sha256", file.sha256.c_str());
    yyjson_mut_obj_add_val(doc, entry, "hashes", hashes);

    yyjson_mut_val *env = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, env, "client", file.env_client.c_str());
    yyjson_mut_obj_add_str(doc, env, "server", file.env_server.c_str());
    yyjson_mut_obj_add_val(doc, entry, "env", env);

    yyjson_mut_val *downloads = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_str(doc, downloads, file.downloadUrl.c_str());
    yyjson_mut_obj_add_val(doc, entry, "downloads", downloads);

    yyjson_mut_arr_append(files, entry);
  }

  yyjson_mut_obj_add_val(doc, root, "files", files);

  char *json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, nullptr);
  std::string result(json);

  free(json);
  yyjson_mut_doc_free(doc);
  return result;
}

// entry point
void packMrpack(const Manifest &m, const std::string &overridesDir,
                const std::string &outPath) {
  int errCode = 0;
  zip_t *archive =
      zip_open(outPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errCode);
  if (!archive)
    throw std::runtime_error("zip: cannot create " + outPath);

  std::cout << "  adding modrinth.index.json\n";
  std::string index = buildIndexJson(m);
  zipAddString(archive, index, "modrinth.index.json");

  // overrides/ directory (if it exists)
  if (fs::exists(overridesDir) && fs::is_directory(overridesDir)) {
    std::cout << "  adding overrides from " << overridesDir << "\n";
    zipAddDir(archive, overridesDir, "overrides");
  } else {
    std::cout << "  no overrides/ directory found, skipping\n";
  }

  if (zip_close(archive) < 0)
    throw std::runtime_error("zip: failed to finalise " + outPath);

  std::cout << "=> wrote " << outPath << "\n";
}

// MultiMC
static std::string buildMmcInstanceCfg(const Manifest &m) {
  return "InstanceType=OneSix\nname=" + m.name + "\n";
}

static std::string buildMmcPackJson(const Manifest &m) {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_obj_add_int(doc, root, "formatVersion", 1);
  yyjson_mut_val *components = yyjson_mut_arr(doc);

  yyjson_mut_val *mcComp = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_str(doc, mcComp, "uid", "net.minecraft");
  yyjson_mut_obj_add_str(doc, mcComp, "version", m.mcVersion.c_str());
  yyjson_mut_obj_add_bool(doc, mcComp, "important", true);
  yyjson_mut_arr_append(components, mcComp);

  std::string loaderUid;
  if (m.modLoader == "forge")
    loaderUid = "net.minecraftforge";
  else if (m.modLoader == "fabric")
    loaderUid = "net.fabricmc.fabric-loader";
  else if (m.modLoader == "quilt")
    loaderUid = "org.quiltmc.quilt-loader";

  if (!loaderUid.empty()) {
    yyjson_mut_val *loaderComp = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, loaderComp, "uid", loaderUid.c_str());
    yyjson_mut_obj_add_str(doc, loaderComp, "version",
                           m.modLoaderVersion.c_str());
    yyjson_mut_arr_append(components, loaderComp);
  }

  yyjson_mut_obj_add_val(doc, root, "components", components);

  char *json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, nullptr);
  std::string result(json);

  free(json);
  yyjson_mut_doc_free(doc);
  return result;
}

void packMultiMC(const Manifest &m, const std::string &overridesDir,
                 const std::string &outPath) {
  int errCode = 0;
  zip_t *archive =
      zip_open(outPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errCode);
  if (!archive)
    throw std::runtime_error("zip: cannot create " + outPath);

  std::cout << "  adding instance.cfg\n";
  zipAddString(archive, buildMmcInstanceCfg(m), "instance.cfg");

  std::cout << "  adding mmc-pack.json\n";
  zipAddString(archive, buildMmcPackJson(m), "mmc-pack.json");

  if (fs::exists(overridesDir) && fs::is_directory(overridesDir)) {
    std::cout << "  adding overrides mapped to .minecraft/\n";
    zipAddDir(archive, overridesDir, ".minecraft");
  }

  std::cout << "  injecting resolved mods from cache into zip...\n";
  for (const auto &file : m.files) {
    std::string filename = fs::path(file.path).filename().string();
    std::string cachePath = ".please-speed-cache/" + filename;

    if (fs::exists(cachePath)) {
      std::string zipDestination = ".minecraft/mods/" + filename;
      std::cout << "    packing mod: " << filename << "\n";
      zipAddFile(archive, cachePath, zipDestination);
    } else {
      std::cerr << "  warning: cached mod file missing: " << cachePath << "\n";
    }
  }

  if (zip_close(archive) < 0)
    throw std::runtime_error("zip: failed to finalise " + outPath);

  std::cout << "=> wrote " << outPath << "\n";
}