#include "resolver.hpp"
#include "manifest.hpp"

#include <cstring>
#include <curl/curl.h>
#include <openssl/evp.h>
#include <yyjson.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// curl helpers
static size_t curlWriteString(char *ptr, size_t size, size_t nmemb,
                              void *userdata) {
  auto *buf = static_cast<std::string *>(userdata);
  buf->append(ptr, size * nmemb);
  return size * nmemb;
}

static size_t curlWriteFile(char *ptr, size_t size, size_t nmemb,
                            void *userdata) {
  auto *f = static_cast<std::ofstream *>(userdata);
  f->write(ptr, size * nmemb);
  return size * nmemb;
}

static std::string httpGet(const std::string &url,
                           const std::vector<std::string> &headers = {}) {
  CURL *curl = curl_easy_init();
  if (!curl)
    throw std::runtime_error("curl_easy_init failed");

  std::string body;
  struct curl_slist *hdrs = nullptr;

  for (const auto &h : headers)
    hdrs = curl_slist_append(hdrs, h.c_str());

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "please-speed/0.1");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    throw std::runtime_error(std::string("curl error: ") +
                             curl_easy_strerror(res));

  return body;
}

// http download
static void httpDownload(const std::string &url, const std::string &destPath,
                         const std::vector<std::string> &headers = {}) {
  fs::create_directories(fs::path(destPath).parent_path());
  std::ofstream f(destPath, std::ios::binary);
  if (!f)
    throw std::runtime_error("cannot open for writing: " + destPath);

  CURL *curl = curl_easy_init();
  if (!curl)
    throw std::runtime_error("curl_easy_init failed");

  struct curl_slist *hdrs = nullptr;
  for (const auto &h : headers)
    hdrs = curl_slist_append(hdrs, h.c_str());

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFile);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &f);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "please-speed/0.1");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    throw std::runtime_error(std::string("curl download error: ") +
                             curl_easy_strerror(res));
}

// hashing

static std::string hashPath(const std::string &path, const EVP_MD *algo) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    throw std::runtime_error("cannot open for hashing: " + path);

  std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

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

static std::string readKeyFromJson(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) return "";

  std::string contents((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

  yyjson_doc* doc = yyjson_read(contents.c_str(), contents.size(), 0);
  if (!doc)
      throw std::runtime_error("invalid JSON in config: " + path);

  yyjson_val* root   = yyjson_doc_get_root(doc);
  yyjson_val* keyVal = yyjson_obj_get(root, "curseforge_api_key");

  std::string key;
  if (keyVal && yyjson_is_str(keyVal))
      key = yyjson_get_str(keyVal);

  yyjson_doc_free(doc);
  return key;
}

// config: CurseForge API key
static std::string getCurseForgeKey() {
  // env var
  const char* env = std::getenv("CURSEFORGE_API_KEY");
  if (env && strlen(env) > 0) return env;

  // local build_config.json
  std::string local = readKeyFromJson("build_config.json");
  if (!local.empty()) return local;

  // global ~/.config/please-speed/config.json
  const char* home = std::getenv("HOME");
  if (home) {
      std::string global = readKeyFromJson(
          std::string(home) + "/.config/please-speed/config.json");
      if (!global.empty()) return global;
  }

  return "";
}

// modrinth resolver

struct ResolvedFile {
  std::string url;
  std::string filename;
  std::string sha512;
  std::string sha256;
  size_t fileSize;
};

static ResolvedFile resolveModrinth(const std::string &slug,
                                    const std::string &mcVersion,
                                    const std::string &modLoader) {
  // POST /v2/project/{slug}/version with filters
  std::string url = "https://api.modrinth.com/v2/project/" + slug +
                    "/version?game_versions=[%22" + mcVersion +
                    "%22]&loaders=[%22" + modLoader + "%22]";

  std::cout << "  [modrinth] resolving " << slug << "...\n";
  std::string body = httpGet(
      url, {"User-Agent: please-speed/0.1 (github.com/you/please-speed)"});

  yyjson_doc *doc = yyjson_read(body.c_str(), body.size(), 0);
  if (!doc)
    throw std::runtime_error("modrinth: invalid JSON for " + slug);

  yyjson_val *root = yyjson_doc_get_root(doc);
  if (!yyjson_is_arr(root) || yyjson_arr_size(root) == 0) {
    yyjson_doc_free(doc);
    throw std::runtime_error("modrinth: no versions found for " + slug +
                             " (mc=" + mcVersion + ", loader=" + modLoader +
                             ")");
  }

  // iterate to find first "release" version type
  ResolvedFile result;
  bool found = false;

  size_t idx, max;
  yyjson_val *version;
  yyjson_arr_foreach(root, idx, max, version) {
    yyjson_val *vtype = yyjson_obj_get(version, "version_type");
    if (!vtype || std::string(yyjson_get_str(vtype)) != "release")
      continue;

    // get first file in this version
    yyjson_val *files = yyjson_obj_get(version, "files");
    if (!files || yyjson_arr_size(files) == 0)
      continue;

    yyjson_val *file = yyjson_arr_get_first(files);

    yyjson_val *urlVal = yyjson_obj_get(file, "url");
    yyjson_val *filenameVal = yyjson_obj_get(file, "filename");
    yyjson_val *sizeVal = yyjson_obj_get(file, "size");
    yyjson_val *hashesVal = yyjson_obj_get(file, "hashes");

    if (!urlVal || !filenameVal)
      continue;

    result.url = yyjson_get_str(urlVal);
    result.filename = yyjson_get_str(filenameVal);
    result.fileSize = sizeVal ? (size_t)yyjson_get_int(sizeVal) : 0;

    if (hashesVal) {
      yyjson_val *s512 = yyjson_obj_get(hashesVal, "sha512");
      if (s512)
        result.sha512 = yyjson_get_str(s512);
    }

    found = true;
    break;
  }

  yyjson_doc_free(doc);

  if (!found)
    throw std::runtime_error("modrinth: no stable release found for " + slug);

  return result;
}

static ResolvedFile resolveCurseForge(const std::string &slug,
                                      const std::string &mcVersion,
                                      const std::string &modLoader) {
  std::string apiKey = getCurseForgeKey();
  if (apiKey.empty())
    throw std::runtime_error("CurseForge API key not set. "
                             "Set CURSEFORGE_API_KEY env var or add it to "
                             "~/.config/please-speed/config");

  std::vector<std::string> headers = {"x-api-key: " + apiKey,
                                      "Accept: application/json"};

  // search for the mod by slug
  std::cout << "  [curseforge] resolving " << slug << "...\n";
  std::string searchUrl = "https://api.curseforge.com/v1/mods/search"
                          "?gameId=432&slug=" +
                          slug;

  std::string searchBody = httpGet(searchUrl, headers);
  yyjson_doc *searchDoc = yyjson_read(searchBody.c_str(), searchBody.size(), 0);
  if (!searchDoc)
    throw std::runtime_error("curseforge: invalid search JSON for " + slug);

  yyjson_val *searchRoot = yyjson_doc_get_root(searchDoc);
  yyjson_val *data = yyjson_obj_get(searchRoot, "data");

  if (!data || yyjson_arr_size(data) == 0) {
    yyjson_doc_free(searchDoc);
    throw std::runtime_error("curseforge: mod not found: " + slug);
  }

  yyjson_val *mod = yyjson_arr_get_first(data);
  yyjson_val *idVal = yyjson_obj_get(mod, "id");
  int64_t modId = yyjson_get_int(idVal);
  yyjson_doc_free(searchDoc);

  std::string filesUrl =
      "https://api.curseforge.com/v1/mods/" + std::to_string(modId) +
      "/files"
      "?gameVersion=" +
      mcVersion + "&modLoaderType=1"; // 1=Forge, 4=Fabric, 5=Quilt

  std::string filesBody = httpGet(filesUrl, headers);
  yyjson_doc *filesDoc = yyjson_read(filesBody.c_str(), filesBody.size(), 0);
  if (!filesDoc)
    throw std::runtime_error("curseforge: invalid files JSON for " + slug);

  yyjson_val *filesRoot = yyjson_doc_get_root(filesDoc);
  yyjson_val *filesData = yyjson_obj_get(filesRoot, "data");

  if (!filesData || yyjson_arr_size(filesData) == 0) {
    yyjson_doc_free(filesDoc);
    throw std::runtime_error("curseforge: no files found for " + slug +
                             " (mc=" + mcVersion + ")");
  }

  yyjson_val *file = yyjson_arr_get_first(filesData);
  yyjson_val *dlUrl = yyjson_obj_get(file, "downloadUrl");
  yyjson_val *fname = yyjson_obj_get(file, "fileName");
  yyjson_val *fsize = yyjson_obj_get(file, "fileLength");

  ResolvedFile result;
  result.url = dlUrl ? yyjson_get_str(dlUrl) : "";
  result.filename = fname ? yyjson_get_str(fname) : slug + ".jar";
  result.fileSize = fsize ? (size_t)yyjson_get_int(fsize) : 0;

  yyjson_doc_free(filesDoc);

  if (result.url.empty())
    throw std::runtime_error(
        "curseforge: no download URL for " + slug +
        " (mod may require manual download due to CurseForge TOS)");

  return result;
}

// entry point
void resolveMods(std::vector<ModFile> &files, const std::string &mcVersion,
                 const std::string &modLoader) {

  curl_global_init(CURL_GLOBAL_DEFAULT);

  for (auto &file : files) {
    std::string slug = file.sha512;
    std::string source = file.sha256;

    file.sha512 = "";
    file.sha256 = "";

    try {
      ResolvedFile resolved;

      if (source == "url") {
        std::cout << "  [url] downloading " << file.downloadUrl << "\n";
        std::string tmpPath =
            ".please-speed-cache/" + fs::path(file.path).filename().string();
        httpDownload(file.downloadUrl, tmpPath);
        file.sha512 = hashPath(tmpPath, EVP_sha512());
        file.sha256 = hashPath(tmpPath, EVP_sha256());
        file.fileSize = fs::file_size(tmpPath);
        continue;
      }

      if (source == "modrinth") {
        resolved = resolveModrinth(slug, mcVersion, modLoader);
      } else if (source == "curseforge") {
        resolved = resolveCurseForge(slug, mcVersion, modLoader);
      } else {
        throw std::runtime_error("unknown source: " + source);
      }

      file.path = "mods/" + resolved.filename;
      file.downloadUrl = resolved.url;
      file.fileSize = resolved.fileSize;

      std::string cachePath = ".please-speed-cache/" + resolved.filename;
      if (!fs::exists(cachePath)) {
        std::cout << "  downloading " << resolved.filename << "\n";
        httpDownload(resolved.url, cachePath);
      } else {
        std::cout << "  cached " << resolved.filename << "\n";
      }

      file.sha512 = resolved.sha512.empty() ? hashPath(cachePath, EVP_sha512())
                                            : resolved.sha512;
      file.sha256 = hashPath(cachePath, EVP_sha256());
      file.fileSize = fs::file_size(cachePath);

    } catch (const std::exception &e) {
      std::cerr << "error resolving " << slug << ": " << e.what() << "\n";
      throw;
    }
  }

  curl_global_cleanup();
}