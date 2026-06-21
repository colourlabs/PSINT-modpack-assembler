# PleaseSpeedINeedThisModpackAssembler

A minecraft modpack assembler because there are already 120,000+ of them already and we need more.

> This is a work in progress, some features might be missing.

## why?

Why not.

## building modpacks

Unlike other modpack bundlers which use generic TOML or JSON configuration options, which for programmers and regular people suck to write, "Please Speed I Need This Modpack Assembler" short hand (**PSINT-modpack-assembler**) uses the [wren](https://wren.io/) scripting language as a nice DSL (with also a programming language) to build modpacks easily.

[wren](https://wren.io/) is quite simple to learn, and for modpack-development you will probably need to understand 3 features of the language.

### getting started

Create a `build.wren` file, and import the base `Modpack` class from the embedded `please-speed` module and create a global variable `modpack` for the assembler to use

```wren
import "please-speed" for Modpack

// extends from the Modpack class
class ExampleModpack is Modpack {
  construct new() {
    // super() setups the bundled init code for you do modpack configuration in
    super()

    // ... initial config goes here
  }
}

// create a global variable `modpack` with a new instance of the class you just created,
var modpack = ExampleModpack.new()
```

From here you can configure Modpack stuff pretty intuitively, for example a Forge Modpack would look like:

```wren
import "please-speed" for Modpack

class ExampleModpack is Modpack {
  construct new() {
    super()

    // modpack name, loader, MC version and other stuff
    name = "My Forge Modpack"
    version_id = "1.0.0"
    mod_loader = "forge"
    mod_loader_version = "47.3.0"
    mc_version = "1.20.1"

    // example performance mods.
    mod("ferritecore").side("both").source("modrinth")
    mod("smoothboot-forge").side("both").source("modrinth")

    // client only (only on the client will this be included)
    mod("journeymap").side("client").source("curseforge")
    mod("jei").side("client").source("curseforge")
    mod("neat").side("client").source("curseforge")

    // server only (this will be bundled in the server zip)
    mod("spark").side("server").source("modrinth")

    // both sides (runs on both MC server and client)
    mod("create").side("both").source("curseforge")
    mod("appliedenergistics2").side("both").source("curseforge")
    mod("thermal-expansion").side("both").source("curseforge")
    mod("iron-chests").side("both").source("curseforge")
  }
}
```

or, if you want a Fabric modpack:

```wren
class ExampleModpack is Modpack {
  construct new() {
    super()

    // modpack name, loader, MC version and other stuff as before
    name = "My Fabric Modpack"
    version_id = "1.0.0"
    mod_loader = "fabric"
    mod_loader_version = "0.16.10"
    mc_version = "1.20.1"

    // same syntax
    mod("sodium").side("client").source("modrinth")
    mod("lithium").side("both").source("modrinth")
  }
}
```

## API reference

### `Modpack` fields

These are set in your constructor with `field = value`.

| Field                | Required | Type   | Description                         |
| -------------------- | -------- | ------ | ----------------------------------- |
| `name`               | yes      | string | Display name of the modpack         |
| `version_id`         | yes      | string | Version string e.g. `"1.0.0"`       |
| `mc_version`         | yes      | string | Minecraft version e.g. `"1.20.1"`   |
| `mod_loader`         | yes      | string | `"forge"`, `"fabric"`, or `"quilt"` |
| `mod_loader_version` | yes      | string | Loader version e.g. `"47.3.0"`      |

### `mod(slug)`, adding mods

Mods are declared with `mod("slug")` and configured via chained method calls. The slug is the identifier from the mod's Modrinth or CurseForge URL.

```wren
mod("sodium").side("client").source("modrinth")
```

> **Note:** the global variable in your `build.wren` must be named `modpack`! the assembler looks for this name specifically.

#### chaining methods

| Method        | Values                                | Default       | Description                                            |
| ------------- | ------------------------------------- | ------------- | ------------------------------------------------------ |
| `.side(s)`    | `"client"`, `"server"`, `"both"`      | `"both"`      | Where the mod is included                              |
| `.source(s)`  | `"modrinth"`, `"curseforge"`, `"url"` | `"modrinth"`  | Where to fetch the mod from                            |
| `.version(v)` | any version string                    | latest stable | Pin a specific mod version                             |
| `.url(u)`     | any HTTPS URL                         | —             | Direct download URL, implicitly sets source to `"url"` |

#### side behaviour

| Side       | Included in client pack | Included in server export |
| ---------- | ----------------------- | ------------------------- |
| `"both"`   | yes                     | yes                       |
| `"client"` | yes                     | no                        |
| `"server"` | no                      | yes                       |

### commands

```
please-speed build server         resolve mods and write manifest.json
please-speed pack modrinth        export a Modrinth .mrpack
please-speed pack prismlauncher   export a Prism Launcher modpack
please-speed pack server          generate server install.sh and install.bat
```

### CurseForge API key

Some mods are exclusive to CurseForge and require an API key to resolve (to make life harder for no reason). Get one at [console.curseforge.com](https://console.curseforge.com).

Keys are resolved in this order:

1. `CURSEFORGE_API_KEY` environment variable
2. `build_config.json` in your project directory
3. `~/.config/please-speed/config.json` global config

**environment variable:**
```bash
export CURSEFORGE_API_KEY=your-key-here
```

**project-level `build_config.json`** (recommended, add to `.gitignore`):
```json
{
  "curseforge_api_key": "your-key-here"
}
```

**global `~/.config/please-speed/config.json`:**
```json
{
  "curseforge_api_key": "your-key-here"
}
```

If a CurseForge mod is missing the key you'll get a clear error at build time.