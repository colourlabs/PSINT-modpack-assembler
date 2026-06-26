#include "manifest.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>

#include "log.hpp"
#include <openssl/evp.h>
#include <sstream>
#include <stdexcept>

#include <yyjson.h>

// SHA512 hash helper
static std::string hashBytes(const std::vector<uint8_t> &data,
                             const EVP_MD *algo) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, algo, nullptr);
  EVP_DigestUpdate(ctx, data.data(), data.size());
  EVP_DigestFinal_ex(ctx, digest, &len);
  EVP_MD_CTX_free(ctx);

  std::ostringstream ss;
  for (unsigned int i = 0; i < len; i++)
    ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
  return ss.str();
}

std::pair<std::string, std::string> hashFile(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    throw std::runtime_error("cannot open file for hashing: " + path);

  std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
  return {hashBytes(data, EVP_sha512()), hashBytes(data, EVP_sha256())};
}

void writeManifest(const Manifest &m, const std::string &outPath) {
  // create doc + root object
  yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
  yyjson_mut_val *root = yyjson_mut_obj(doc);
  yyjson_mut_doc_set_root(doc, root);

  // top-level fields
  yyjson_mut_obj_add_str(doc, root, "formatVersion", m.formatVersion.c_str());
  yyjson_mut_obj_add_str(doc, root, "game", m.game.c_str());
  yyjson_mut_obj_add_str(doc, root, "name", m.name.c_str());
  yyjson_mut_obj_add_str(doc, root, "versionId", m.versionId.c_str());

  // dependencies object
  yyjson_mut_val *deps = yyjson_mut_obj(doc);
  yyjson_mut_obj_add_str(doc, deps, "minecraft", m.mcVersion.c_str());
  std::string loaderKey = m.modLoader + "-loader";
  yyjson_mut_obj_add_str(doc, deps, loaderKey.c_str(),
                         m.modLoaderVersion.c_str());
  yyjson_mut_obj_add_val(doc, root, "dependencies", deps);

  // files array
  yyjson_mut_val *files = yyjson_mut_arr(doc);
  for (const auto &file : m.files) {
    yyjson_mut_val *entry = yyjson_mut_obj(doc);

    yyjson_mut_obj_add_str(doc, entry, "path", file.path.c_str());
    yyjson_mut_obj_add_int(doc, entry, "fileSize", (int64_t)file.fileSize);

    // hashes
    yyjson_mut_val *hashes = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, hashes, "sha512", file.sha512.c_str());
    yyjson_mut_obj_add_str(doc, hashes, "sha256", file.sha256.c_str());
    yyjson_mut_obj_add_val(doc, entry, "hashes", hashes);

    // env
    yyjson_mut_val *env = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, env, "client", file.env_client.c_str());
    yyjson_mut_obj_add_str(doc, env, "server", file.env_server.c_str());
    yyjson_mut_obj_add_val(doc, entry, "env", env);

    // downloads array
    yyjson_mut_val *downloads = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_str(doc, downloads, file.downloadUrl.c_str());
    yyjson_mut_obj_add_val(doc, entry, "downloads", downloads);

    yyjson_mut_arr_append(files, entry);
  }

  yyjson_mut_obj_add_val(doc, root, "files", files);

  yyjson_write_flag flags = YYJSON_WRITE_PRETTY;
  yyjson_write_err err;
  char *json = yyjson_mut_write_opts(doc, flags, nullptr, nullptr, &err);
  if (!json) {
    yyjson_mut_doc_free(doc);
    throw std::runtime_error(std::string("yyjson write error: ") + err.msg);
  }

  std::ofstream f(outPath);
  if (!f) {
    free(json);
    yyjson_mut_doc_free(doc);
    throw std::runtime_error("cannot write " + outPath);
  }

  f << json << "\n";
  free(json);
  yyjson_mut_doc_free(doc);

  logging::step("wrote " + outPath);
}
