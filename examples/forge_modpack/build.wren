import "please-speed" for Modpack

class ForgeModpack is Modpack {
  construct new() {
    super()

    name = "My Forge Modpack"
    version_id = "1.0.0"
    mod_loader = "forge"
    mod_loader_version = "47.3.0"
    mc_version = "1.20.1"

    // performance
    mod("ferritecore").side("both").source("modrinth")
    mod("smoothboot-forge").side("both").source("modrinth")

    // client only
    mod("journeymap").side("client").source("curseforge")
    mod("jei").side("client").source("curseforge")
    mod("neat").side("client").source("curseforge")

    // server only
    mod("spark").side("server").source("modrinth")

    // both sides
    mod("create").side("both").source("curseforge")
    mod("appliedenergistics2").side("both").source("curseforge")
    mod("thermal-expansion").side("both").source("curseforge")
    mod("iron-chests").side("both").source("curseforge")
  }
}

// modpack variable is required for please-speed to build it
var modpack = ForgeModpack.new()