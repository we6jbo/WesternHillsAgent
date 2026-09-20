# Packaging notes

The source application can build natively with Qt 6, CMake and Ninja.

## Flatpak

The local manifest is:

`packaging/flatpak/org.j03.WesternHillsAgent.yml`

It uses the KDE Qt 6 runtime and grants network sharing because WesternHillsAgent
binds loopback TCP endpoints.

A sandboxed Flatpak does not automatically receive the host access that the
native application has to JeremiahPortGuard data under `/var/lib/...` or to
`/opt/web213`. When that registry is inaccessible the current application code
can use its temporary-port fallback. Store publication should not add broad host
filesystem access merely to reproduce native-machine behavior without a separate
review.

For a Flathub submission, replace the local `type: dir` source with an immutable
public source archive or Git commit and add a project icon/screenshot metadata
before submission.

## Snap

The generated `snapcraft.yaml` uses strict confinement, core24 and the
`kde-neon-6` extension. The network/network-bind interfaces cover the local TCP
service. Strict confinement likewise means machine-local `/var/lib` and `/opt`
integration may differ from the native build.

Before a stable Snap Store release, register the final snap name and test the
strictly confined build on a clean machine.
