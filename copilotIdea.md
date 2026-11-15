What an overlay icon is
Many KDE applications internally create icons like:

- Folder + lock
- Folder + sync
- Folder + emblem
- Network drive + warning

Conceptually:
``` Plain Text
┌─────────────┐
│   Folder    │
│         MD  │
└─────────────┘
```
The base icon remains theme-controlled, while a smaller emblem is painted on top.
As far as I'm aware, there is no existing KDE/Dolphin plugin that does exactly "folder icon + file-type badge based on folder contents" automatically.

There are a few related projects:

- Dolphin Color Tags adds colored dots to file and folder icons using a Dolphin plugin. It demonstrates that overlay badges on folder icons are possible within Dolphin/Plasma 6.
- Dolphin Folder Color lets you quickly assign colored folder icons, but it doesn't classify folders by contents.
- Dolphin and Baloo support tags on files and folders, which can be searched and displayed, but they are metadata tags rather than automatic language/content badges.

What you're describing is closer to:

``` Plain Text
~/projects/kernel -> folder + C badge
~/projects/neovim -> folder + Lua/Neovim badge
~/notes/physics -> folder + Markdown badge
~/projects/server -> folder + Rust badge
```
and Dolphin does not appear to have a built-in feature or well-known plugin for that. The search results only turned up manual folder icon customization, color-tag plugins, and metadata tagging features.

### The closest existing solution

If I were implementing this today, I'd actually build it as a Dolphin plugin named FolderBadges similar to Dolphin Color Tags:

1. Scan folder contents.
1. Determine dominant type.
1. Store classification in an xattr or .directory.
1. Draw a small overlay badge during icon rendering.

That would automatically follow icon-theme changes because the plugin would request the current theme's folder and text-markdown, text-rust, etc. icons at render time rather than generating PNGs. This is essentially the "overlay icon" idea I mentioned earlier.

If I were actually developing FolderBadges, I'd split it into two pieces: 
1. `folderbadgesd` A background classifier that determines:
   - rust
   - typescript
   - markdown
   - nix
   - neovim
   - termux
   and stores the results.
2. `Dolphin FolderBadges plugin` which simply brenders the badge overlay.
That way another frontend could later be written for:

- Plasma desktop icons
- KDE file dialogs
- Krusader

without duplicating the classification logic.

So the first version would probably be Dolphin-only, but the underlying classification system wouldn't have to be.
