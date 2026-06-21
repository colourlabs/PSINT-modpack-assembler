class Mod {
  construct new(slug) {
    _slug = slug
    _version = null
    _side = "both"        // "client", "server", "both"
    _source = "modrinth"  // "modrinth", "curseforge", "url"
    _url = null
  }
  
  version(v) { 
    _version = v
    return this 
  }

  side(s) {
    _side = s 
    return this
  }
  
  source(s) {
    _source = s 
    return this
  }
  
  url(u) {
    _url = u
    _source = "url"
    return this
  }

  slug { _slug }
  version { _version }
  side { _side }
  source { _source }
  url { _url }
}

class Modpack {
  construct new() {
    _mod_loader = null
    _mod_loader_version = null
    _mc_version = null
    _mods = []
  }

  name=(v) { _name = v }
  version_id=(v) { _version_id = v }
  mod_loader=(v) { _mod_loader = v }
  mod_loader_version=(v) { _mod_loader_version = v }
  mc_version=(v) { _mc_version = v }

  mod_loader { _mod_loader }
  mod_loader_version { _mod_loader_version }
  mc_version { _mc_version }
  mods { _mods }
  name { _name }
  version_id { _version_id }

  // mod("slug") returns a Mod
  mod(slug) {
    var m = Mod.new(slug)
    _mods.add(m)
    return m
  }
}