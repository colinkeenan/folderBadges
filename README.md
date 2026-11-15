# FolderBadges

Folder icons in Dolphin get a small badge showing what the folder mostly contains:

```
~/projects/kernel   -> folder + C badge
~/projects/neovim   -> folder + Neovim badge
~/notes/physics     -> folder + Markdown badge
~/projects/server   -> folder + Rust badge
```

The badge is an *overlay*: the plugin only hands Dolphin an icon name and Dolphin paints
it on top of whatever folder icon the current theme has, so no PNGs are generated and
folders keep following the icon theme.

The badges themselves are small coloured labels (`RS`, `TS`, `PY`, a Markdown mark, ...)
shipped with FolderBadges in [data/badges](data/badges), because the file-type icons of
most themes are a page with a tiny logo that is unreadable at badge size. They are
installed as `folderbadges-<type>` emblems in the hicolor theme; an icon theme can ship
icons with the same names to restyle them. Neovim uses the real `nvim` application icon
when it is installed.

The original idea is in [copilotIdea.md](copilotIdea.md).

## Pieces

| Piece | What it does |
|---|---|
| `folderbadgesd` | Background classifier. Started on demand through D-Bus, scans folders off the GUI thread, stores the result, exits after ten idle minutes. |
| `folderbadgesoverlay.so` | Dolphin/KIO overlay icon plugin (`kf6/overlayicon`). Asks the daemon, caches the answer, returns the badge icon. Contains no classification logic. |
| `folderbadges` | Command line tool: dry-run the classifier, pin a badge, go back to automatic. |

Another frontend (Plasma desktop icons, Krusader, ...) only needs to talk to the daemon.

## Build and install

Needs Qt 6, KDE Frameworks 6 (KIO, KCoreAddons) and extra-cmake-modules.

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j
ctest --test-dir build
sudo cmake --install build
```

Then restart Dolphin (`killall dolphin`, or close every window). After an upgrade also run
`pkill folderbadgesd` so the new daemon is used. Folders get their badge a
moment after they first show up in a view.

To uninstall: `sudo xargs rm < build/install_manifest.txt`.

## How a folder is classified

1. **Markers and names**, first match in type-table order wins: `Cargo.toml` -> Rust,
   `tsconfig.json` -> TypeScript, `package.json` -> JavaScript, `flake.nix` -> Nix,
   `init.lua` or a folder called `nvim` / `*.nvim` -> Neovim, ...
   A marker shared by several types only narrows the choice: `CMakeLists.txt` or
   `meson.build` means C or C++, whichever has more files.
2. If the files directly in the folder (at least two; hidden files and symlinks ignored) all have the
   **same type**, that type wins and subfolders are not looked at. Markdown notes next to a
   folder full of images are Markdown.
3. Otherwise the **dominant file type** of a bounded breadth-first scan (4 levels, 5000
   entries; hidden entries, symlinks and `node_modules`, `target`, `build`, ... are skipped).
   The winner must make up at least 40% of all files seen and must not be tied.
   `.h` files count for C or C++, whichever is ahead.

No match means no badge. See what would happen without changing anything:

```sh
folderbadges classify ~/projects/*
```

## Where results are stored

In the `user.folderbadges` extended attribute of the folder itself:

```
$ getfattr -n user.folderbadges ~/projects/server
user.folderbadges="1;rust;1794752900;auto"      # version;type;classified-at;auto|pinned
```

An automatic result is re-checked when the folder's own modification time is newer than
the stamp (something was added to or removed from it directly) or after 24 hours.
For folders where the attribute cannot be written (read-only, filesystem without xattrs)
the daemon keeps the result in memory only.

## Overriding

```sh
folderbadges set ~/notes/physics latex   # pin a badge
folderbadges set ~/Downloads none        # pin "no badge"
folderbadges clear ~/notes/physics       # back to automatic
folderbadges clear -r ~/Documents         # same for a folder and everything below it;
                                         # skips hidden folders, keeps pinned badges
folderbadges types                       # list known types and their icons
```

Open Dolphin windows update right away.

## Configuration

Both files are optional. Restart the daemon after editing (`pkill folderbadgesd`; it is
started again when needed).

`~/.config/folderbadges/types.json` adds types or replaces built-in ones by `id`.
The format is the same as [data/types.json](data/types.json):

```json
{ "types": [
  { "id": "zig", "name": "Zig", "icons": ["text-x-zig"],
    "markers": ["build.zig"], "extensions": ["zig"] }
] }
```

`~/.config/folderbadges/folderbadges.conf`:

```ini
maxDepth=4
maxEntries=5000
minShare=0.4
sameTypeMinFiles=2
maxAgeHours=24
ignoredDirs=node_modules, target, build, dist, vendor, __pycache__, venv, third_party
excludedPaths=/proc, /sys, /dev
```

## D-Bus interface

Service `io.github.folderbadges`, object `/FolderBadges`, interface
`io.github.folderbadges.Classifier`:

- `Query(s path) -> (s type, as icons)`: badge of a folder, classifying it if needed.
  `icons` are candidate theme icon names, best first.
- `Refresh(s path)`: re-read / re-classify and emit `BadgeChanged`.
- signal `BadgeChanged(s path, s type, as icons)`

## Known limits

- Changes deeper inside a folder are only noticed at the 24 hour re-check
  (`folderbadges clear DIR` forces one).
- Dolphin places the badge in the first free corner (bottom right on a plain folder); a
  symlinked folder already uses that corner for its link emblem.
- The badge icons are plain labels drawn for this project, not the official logos.
  To change one, edit `data/badges/generate.py` and run it, or list a different icon
  first in the type's `icons` in `~/.config/folderbadges/types.json`.

## License

GPL-3.0-or-later, see [LICENSE](LICENSE).
