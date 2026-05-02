# Mutagen
Tool to live sync a directory over ssh.
- [Documentation](https://mutagen.io/documentation/synchronization/).
- Current Configuration Syncs to ROME
- Ignores:
    - `build/*`
    - `build_BF/*`
    - `.cache/*`
    - `.git/*`

## Install
[Web Install](https://webinstall.dev/mutagen/)
```sh
curl -sS https://webi.sh/mutagen | sh; \
source ~/.config/envman/PATH.env
```
- Places the install files into 
	- `~/.local/bin`
	- `~/.mutagen.yml`


## Usage
- Start Mutagen
```sh
mutagen project start
```

- Stop Mutagen
```sh
mutagen project start
```

- View Status
```sh
mutagen project list
```

## Configuration
Controlled via `mutagen.yml`.
Further documentation can be found on the official pages.
See [Basic Configuration](https://mutagen.io/documentation/introduction/configuration)
and [Project Configuration](https://mutagen.io/documentation/orchestration/projects/).
