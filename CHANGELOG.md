## [1.0.0] - 2025-03-24
### Added
- Initial release of the project

### Changed
- NTR

### Fixed
- NTR

## [1.1.0] - 2026-09-10
### Added
- Added support for matching administrative units at a user-specified administrative level.
- Added a command-line parameter to specify the target database name.
- Added support for additional countries: Austria (AT), Czechia (CZ), Spain (ES) and Liechtenstein (LI).
- Split the processing into independent steps, allowing intermediate results to be stored and reused in dedicated PostGIS tables.

### Changed
- Updated the configuration of the highest administrative level (lowest_level) for the supported countries.
- Updated the SOCLE dependency.
- Changed the output table suffix so that it is now based only on the user-defined suffix instead of combining it with the country code.
- Adapted the application to the IGN-MUT deployment environment.

### Fixed
- Fixed snapping of administrative units to international borders.
- Fixed issues in processing steps 610 and 620.
- Excluded destroyed objects from the processing.
- Fixed memory leaks and optimized memory usage following Valgrind analysis.