# Agent Instructions

- Keep token use in mind.
- Avoid adding code when existing code can be reused or adapted.
- Never add UI elements without approval; reuse existing documented UI patterns.
- If you notice code or UI that can be removed or consolidated, mention it.

## Versioning And Git

- By default, every meaningful code or documentation change must bump the patch version.
- Bump release identity through component Kconfig defaults.
- Bump component package version through `components/esp32_c6_zigbee_base/idf_component.yml`.
- Keep dependency examples pinned to exact tags.
- For each version bump:
  - Increment `SWBuildID` using SemVer without the leading `v`.
  - Increment `ApplicationVersion`.
  - Update exact dependency pins from `vX.Y.Z` to the new tag.
  - Update `DateCode` only when the release-intended build date changes.
- Build `examples/generic_device` before committing.
- Commit successful changes by default.
- Push after committing unless the user explicitly says not to or the push fails.
- Do not create release tags unless the user explicitly asks for a release/tag.
