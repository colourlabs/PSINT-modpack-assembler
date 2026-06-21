#pragma once
#include <string>
#include <vector>

struct ModFile {
  std::string path; // "mods/sodium-0.5.jar"
  std::string downloadUrl;
  std::string sha512;
  std::string sha256;
  size_t fileSize;
  std::string env_client; // "required", "optional", "unsupported"
  std::string env_server;
};

struct Manifest {
  std::string formatVersion = "1";
  std::string game = "minecraft";
  std::string name;
  std::string versionId;
  std::string mcVersion;
  std::string modLoader; // "forge", "fabric", "quilt"
  std::string modLoaderVersion;
  std::vector<ModFile> files;
};

void writeManifest(const Manifest &manifest, const std::string &outPath);
