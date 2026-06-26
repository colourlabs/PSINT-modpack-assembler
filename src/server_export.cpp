#include "server_export.hpp"
#include "manifest.hpp"

#include <filesystem>
#include <fstream>

#include "log.hpp"
#include <string>

namespace fs = std::filesystem;

// returns the minor version number, e.g. "1.16.5" -> 16, "1.20.1" -> 20
// returns -1 if it can't be parsed
static int parseMcMinor(const std::string &mcVersion) {
  // expects "1.X" or "1.X.Y"
  size_t firstDot = mcVersion.find('.');
  if (firstDot == std::string::npos)
    return -1;
  size_t secondDot = mcVersion.find('.', firstDot + 1);
  std::string minorStr =
      mcVersion.substr(firstDot + 1, secondDot - firstDot - 1);
  try {
    return std::stoi(minorStr);
  } catch (...) {
    return -1;
  }
}

// true if this Forge version relies on the generated run.sh/run.bat
// launcher instead of producing a directly-runnable server jar
// (1.17+; also the fallback if the mc version couldn't be parsed)
static bool forgeUsesRunScript(const Manifest &m) {
  if (m.modLoader != "forge")
    return false;
  int minor = parseMcMinor(m.mcVersion);
  return minor < 0 || minor >= 17;
}

std::string getJarServerFileName(const Manifest &m) {
  std::string targetJar = "server.jar";

  if (m.modLoader == "forge") {
    int minor = parseMcMinor(m.mcVersion);
    if (minor >= 0 && minor < 17) {
      // pre-1.17 Forge installer produces a runnable universal jar
      targetJar = "forge-" + m.mcVersion + "-" + m.modLoaderVersion +
                  "-universal.jar";
    }
    // 1.17+ doesn't produce a runnable jar at all, handled via
    // forgeUsesRunScript() in writeBatch/writeShell instead
  } else if (m.modLoader == "fabric") {
    targetJar = "fabric-server-launch.jar";
  } else if (m.modLoader == "quilt") {
    targetJar = "quilt-server-launch.jar";
  }

  return targetJar;
}

// mod loader installer URLs
static std::string forgeInstallerUrl(const std::string &mc,
                                     const std::string &loader) {
  return "https://maven.minecraftforge.net/net/minecraftforge/forge/" + mc +
         "-" + loader + "/forge-" + mc + "-" + loader + "-installer.jar";
}

static std::string fabricInstallerUrl() {
  return "https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.0.0/"
         "fabric-installer-1.0.0.jar";
}

static std::string quiltInstallerUrl() {
  return "https://quiltmc.org/api/v1/download-latest-installer/java-universal";
}

// batch script (Windows)
static void writeBatch(const Manifest &m, const std::string &path) {
  std::ofstream f(path);

  std::string loaderUrl;
  std::string installCmd;
  if (m.modLoader == "forge") {
    loaderUrl = forgeInstallerUrl(m.mcVersion, m.modLoaderVersion);
    installCmd = "java -jar loader-installer.jar --installServer";
  } else if (m.modLoader == "fabric") {
    loaderUrl = fabricInstallerUrl();
    installCmd = "java -jar loader-installer.jar server -mcversion " +
                 m.mcVersion + " -loader " + m.modLoaderVersion +
                 " -downloadMinecraft";
  } else if (m.modLoader == "quilt") {
    loaderUrl = quiltInstallerUrl();
    installCmd = "java -jar loader-installer.jar install server " +
                 m.mcVersion + " " + m.modLoaderVersion + " --download-server";
  }

  f << "@echo off\n";
  f << "setlocal\n\n";

  // check Java
  f << "echo checking for Java...\n";
  f << "java -version >nul 2>&1\n";
  f << "if errorlevel 1 (\n";
  f << "  echo ERROR: Java is not installed or not in PATH.\n";
  f << "  echo Download Java from https://adoptium.net or any other JDK\n";
  f << "  pause\n";
  f << "  exit /b 1\n";
  f << ")\n\n";

  // download mod loader installer
  f << "echo downloading " << m.modLoader << " installer...\n";
  f << "curl -L -o loader-installer.jar \"" << loaderUrl << "\"\n\n";

  // install mod loader
  f << "echo installing " << m.modLoader << "...\n";
  f << installCmd << "\n\n";

  // download mods
  f << "echo downloading mods...\n";
  f << "if not exist mods mkdir mods\n\n";

  for (const auto &file : m.files) {
    // skip client-only mods
    if (file.env_server == "unsupported")
      continue;
    f << "curl -L -o \"" << file.path << "\" \"" << file.downloadUrl << "\"\n";
  }

  std::string finalJarName = getJarServerFileName(m);

  if (forgeUsesRunScript(m)) {
    f << "\necho done! after install finishes, run 'run.bat' to start the "
         "server.\n";
  } else {
    f << "echo java -Xmx4G -Xms4G -jar " << finalJarName
      << " nogui > start.bat\n";
    f << "\necho done! double-click 'start.bat' to start the server.\n";
  }

  f << "pause\n";
}

// shell script (Linux/macOS)

static void writeShell(const Manifest &m, const std::string &path) {
  std::ofstream f(path);

  std::string loaderUrl;
  std::string installCmd;
  if (m.modLoader == "forge") {
    loaderUrl = forgeInstallerUrl(m.mcVersion, m.modLoaderVersion);
    installCmd = "java -jar loader-installer.jar --installServer";
  } else if (m.modLoader == "fabric") {
    loaderUrl = fabricInstallerUrl();
    installCmd = "java -jar loader-installer.jar server -mcversion " +
                 m.mcVersion + " -loader " + m.modLoaderVersion +
                 " -downloadMinecraft";
  } else if (m.modLoader == "quilt") {
    loaderUrl = quiltInstallerUrl();
    installCmd = "java -jar loader-installer.jar install server " +
                 m.mcVersion + " " + m.modLoaderVersion + " --download-server";
  }

  f << "#!/usr/bin/env bash\n";
  f << "set -euo pipefail\n\n";

  // check Java
  f << "echo \"checking for Java...\"\n";
  f << "if ! command -v java &>/dev/null; then\n";
  f << "  echo \"ERROR: Java is not installed or not in PATH.\"\n";
  f << "  echo \"Download Java from https://adoptium.net or any other JDK "
       "distro\"\n";
  f << "  exit 1\n";
  f << "fi\n\n";

  f << "JAVA_VER=$(java -version 2>&1 | head -1)\n";
  f << "echo \"found: $JAVA_VER\"\n\n";

  // download mod loader installer
  f << "echo \"downloading " << m.modLoader << " installer...\"\n";
  f << "curl -L -o loader-installer.jar \"" << loaderUrl << "\"\n\n";

  // install mod loader
  f << "echo \"installing " << m.modLoader << "...\"\n";
  f << installCmd << "\n\n";

  // download mods
  f << "echo \"downloading mods...\"\n";
  f << "mkdir -p mods\n\n";

  for (const auto &file : m.files) {
    if (file.env_server == "unsupported")
      continue;
    f << "curl -L -o \"" << file.path << "\" \\\n";
    f << "  \"" << file.downloadUrl << "\"\n";
  }

  std::string finalJarName = getJarServerFileName(m);

  if (forgeUsesRunScript(m)) {
    f << "echo \"done! after install finishes, run './run.sh' to start the "
         "server.\"\n";
  } else {
    f << "echo \"#!/usr/bin/env bash\" > start.sh\n";
    f << "echo \"java -Xmx4G -Xms4G -jar " << finalJarName
      << " nogui\" >> start.sh\n";
    f << "chmod +x start.sh\n";

    f << "\necho \"done! run './start.sh' to start the server.\"\n";
  }
  f.close();

  // make it executable
  fs::permissions(path,
                  fs::perms::owner_exec | fs::perms::group_exec |
                      fs::perms::others_exec,
                  fs::perm_options::add);
}

void writeServerExport(const Manifest &m, const std::string &outDir) {
  fs::path outputFolder(outDir);
  fs::create_directories(outputFolder);

  fs::path batchPath = outputFolder / "install.bat";
  fs::path shellPath = outputFolder / "install.sh";

  logging::info("writing " + batchPath.string());
  writeBatch(m, batchPath.string());
  logging::info("writing " + shellPath.string());
  writeShell(m, shellPath.string());

  logging::step("server export written to " + outDir);
}