# NeurOS packaging

The distribution's development package recipe is at
`/home/neuro/real_home/NeurOS/NeurOS-Packages/packages/retro-bundle/` on the
development machine. It builds a pinned Git snapshot, stages the seven games
with namespaced `/usr/bin/retro-bundle-*` commands and desktop entries, and
installs shared resources separately from executables. Its README contains the
archive and package build commands. The source checkout's normal `make` and
`bins/` workflow remains available.

This package is not yet included in the ISO or installer. Before redistribution,
review the origins and rights of bundled models, artwork and other assets, then
verify all games from the packaged layout on an installed graphical system and
perform a clean-environment build. The MIT project license does not establish
third-party asset rights. See the distribution's `NOTES.md` for the full plan.
