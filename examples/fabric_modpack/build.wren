import "please-speed" for Modpack

class ExampleModpack is Modpack {
  construct new() {
    super()

    name = "Test Modpack"
    version_id = "1.0.0"
    mod_loader = "fabric"
    mod_loader_version = "0.16.10"
    mc_version = "1.20.1"

    mod("sodium")
      .side("client")
      .source("modrinth")
    
    mod("lithium")
      .side("both")
      .source("modrinth")
  }
}

// modpack variable is required for please-speed to build it
var modpack = ExampleModpack.new()