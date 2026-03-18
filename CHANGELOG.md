# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

## [1.0.0] - 2026-03-18

### Added
- Initial VRML viewer with OpenGL 3.3 renderer
- VRML 1.0 and VRML 2.0 parser support
- Interactive camera controls (orbit, pan, zoom)
- Wireframe mode toggle
- Material support (diffuse, specular, emissive colors with lighting)
- Sample VRML files for testing
- README.md with project documentation
- test_samples.bat for batch testing VRML files

### Changed
- Update samples: add cube_v2.wrl and face_v1.vrml

### Fixed
- VRML2 parser: support appearance/material DEF/USE and multi-value fields
- Transform hierarchy support: apply translation/rotation/scale to vertices

### Removed
- samples/snowman_v2.wrl

---

## Commit History

| Commit | Date | Description |
|--------|------|-------------|
| 2e4ac25 | 2026-03-18 | Update samples: add cube_v2.wrl and face_v1.vrml, remove snowman_v2.wrl |
| 9b1a841 | 2026-03-18 | Add Transform hierarchy support: apply translation/rotation/scale to vertices |
| 68a4c3e | 2026-03-18 | Fix VRML2 parser: support appearance/material DEF/USE and multi-value fields |
| b928fd4 | 2026-03-18 | Add README.md with project documentation |
| ba0d8a8 | 2026-03-18 | Initial commit: VRML viewer with parser and OpenGL renderer |
