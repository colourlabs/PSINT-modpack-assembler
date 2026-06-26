#include "packer.hpp"
#include "manifest.hpp"

#include <yyjson.h>
#include <zip.h>

#include <algorithm>
#include <filesystem>

#include "log.hpp"
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

    logging::info("adding override: " + zipPath);
    zipAddFile(archive, entry.path().string(), zipPath);
  }
}

// modrinth index format

static std::string buildIndexJson(const Manifest &m) {
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  yyjson_mut_obj_add_int(doc, root, "formatVersion", 1);
  yyjson_mut_obj_add_str(doc, root, "game", "minecraft");
  yyjson_mut_obj_add_str(doc, root, "name", m.name.c_str());
  yyjson_mut_obj_add_str(doc, root, "versionId", m.versionId.c_str());

  yyjson_mut_val *deps = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_str(doc, deps, "minecraft", m.mcVersion.c_str());
 
  std::string loaderKey;
  if (m.modLoader == "fabric")
      loaderKey = "fabric-loader";
  else if (m.modLoader == "quilt")
      loaderKey = "quilt-loader";
  else
      loaderKey = m.modLoader;
  yyjson_mut_obj_add_str(doc, deps, loaderKey.c_str(), m.modLoaderVersion.c_str());

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

void packMrpack(const Manifest &m, const std::string &overridesDir,
                const std::string &outPath) {
  int errCode = 0;
  zip_t *archive =
      zip_open(outPath.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &errCode);
  if (!archive)
    throw std::runtime_error("zip: cannot create " + outPath);

  logging::info("adding modrinth.index.json");
  std::string index = buildIndexJson(m);
  zipAddString(archive, index, "modrinth.index.json");

  if (fs::exists(overridesDir) && fs::is_directory(overridesDir)) {
    logging::info("adding overrides from " + overridesDir);
    zipAddDir(archive, overridesDir, "overrides");
  } else {
    logging::info("no overrides/ directory found, skipping");
  }

  if (zip_close(archive) < 0)
    throw std::runtime_error("zip: failed to finalise " + outPath);

  logging::step("wrote " + outPath);
}

// MultiMC / Prism

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

  std::string instanceCfg = buildMmcInstanceCfg(m);
  std::string mmcPackJson = buildMmcPackJson(m);

  logging::info("adding instance.cfg");
  zipAddString(archive, instanceCfg, "instance.cfg");

  logging::info("adding mmc-pack.json");
  zipAddString(archive, mmcPackJson, "mmc-pack.json");

  if (fs::exists(overridesDir) && fs::is_directory(overridesDir)) {
    logging::info("adding overrides mapped to .minecraft/");
    zipAddDir(archive, overridesDir, ".minecraft");
  }

  logging::info("injecting resolved mods from cache into zip...");
  for (const auto &file : m.files) {
    // skip server-only mods from client pack
    if (file.env_client == "unsupported")
      continue;

    std::string filename = fs::path(file.path).filename().string();
    std::string cachePath = ".please-speed-cache/" + filename;

    if (fs::exists(cachePath)) {
      logging::info("packing mod: " + filename);
      zipAddFile(archive, cachePath, ".minecraft/mods/" + filename);
      if (zip_get_error(archive)->zip_err != 0)
        throw std::runtime_error("zip error after packing :( " + filename +
                                 ": " +
                                 zip_error_strerror(zip_get_error(archive)));
    } else {
      logging::warn("cached mod file missing: " + cachePath);
    }
  }

  if (zip_close(archive) < 0)
    throw std::runtime_error("zip: failed to finalise " + outPath);

  logging::step("wrote " + outPath);
}