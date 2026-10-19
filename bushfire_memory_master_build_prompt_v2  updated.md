# MASTER BUILD PROMPT (v3.0 — C++20 & LINUX DOCKER EDITION) — BUSHFIRE MEMORY
## Australian Bushfire Intelligence, Satellite Search & Historical Analysis Platform

### YOUR ROLE & EXECUTION ENVIRONMENT MANDATE
Act as a principal C++20 systems & geospatial architect, remote-sensing engineer, data engineer, security lead, and QA lead.

Build a complete, working, production-grade **C++20 application** named **Bushfire Memory** and publish the repository to `https://github.com/vishalvermauts/Bushfire-Memory.git`.

- **STRICT LANGUAGE MANDATE (100% C++20 — NO NODE.JS / NO VITE / NO TYPESCRIPT)**:
  - This is a **native C++20 project** built with **CMake (`>= 3.20`)**, **`g++` / `clang++`**, **`libcurl`**, **`SQLite3` (`libsqlite3`)**, **`nlohmann/json`**, and **`cpp-httplib`** (single-header C++ HTTP server) serving an embedded MapLibre GL JS geospatial console over port `8080`.
  - **Do NOT** initialize a Node.js, npm, Vite, React, or TypeScript project. Do not run `npm`, `npx`, or `node`.
- **NO LOCAL WINDOWS COMPILATION — LINUX DOCKER SANDBOX ONLY**:
  - **Never compile on the local Windows host PC** (do not invoke local MSVC, MinGW, or local host builds).
  - Perform all compilation, unit testing, integration verification, and binary execution inside the online Linux Docker environment at [Docker Agentic Platform Sandboxes](https://www.google.com/search?q=https%3A%2F%2Fagentic-platform.docker.com%2Fsandboxes%2Fnew) (`https://agentic-platform.docker.com/sandboxes`) or a Linux Docker container (`ubuntu:24.04` / `debian:bookworm-slim`).
  - **Container Lifecycle & Pause Rule**: Spin up or resume the Linux Docker container to compile (`cmake -B build -G Ninja && cmake --build build`) and run verification suites (`ctest` / `./build/bushfire_memory_tests`), and **pause the container immediately whenever idle or once build/test verification finishes** so cloud sandbox compute is conserved.
- **GITHUB REPOSITORY SYNC MANDATE**:
  - Target repository: `https://github.com/vishalvermauts/Bushfire-Memory.git`
  - Configure git remotes and push clean, atomic commits containing the complete C++20 source tree, `CMakeLists.txt`, `Dockerfile`, `docker-compose.yml`, `.gitignore` (excluding `build/`, `*.sqlite3`, `.env`, and compiled binaries), and the certification report to `https://github.com/vishalvermauts/Bushfire-Memory.git`.
- **Mandatory Free-Only Constraint**: The application must operate using **100% free public APIs, open government datasets, keyless public tile services, and open-source C++ libraries**. Do not require paid Google Maps APIs, paid AI inference, Firebase, paid Mapbox tokens, or billable cloud databases.

## 1. PRODUCT VISION & CRITICAL SAFETY DISTINCTION

**Bushfire Memory** is a C++20 Australian bushfire intelligence, satellite hotspot analysis, and historical exploration platform designed for residents, researchers, environmental organisations, students, journalists, and community groups to:
1. Ingest and explore reported NSW and Australian bushfire incidents with full source provenance.
2. Inspect satellite-detected thermal hotspots and understand sensor confidence vs. ground truth.
3. Compare current fire activity against historical bushfire perimeters and severity datasets.
4. Search and compare pre-fire and post-fire satellite imagery scenes and spectral burn indices.
5. Investigate vegetation impact using official government burnt-area and fire-severity layers.
6. Discover historically similar bushfires using a native C++20 multi-factor similarity scoring engine.
7. Audit data freshness, HTTP fetch latency, SQLite3 cache TTLs, and dataset limitations in real time.
8. Export reproducible research reports (Markdown/print-ready HTML), CSV tables, and RFC 7946 GeoJSON layers with complete licence and source citations.

### Critical Public Safety Distinction (Mandatory Non-Dismissible Rule)
- **Not an Emergency Warning System**: Bushfire Memory is an observational research and historical analysis platform, **not** an emergency dispatch service, evacuation router, or official warning authority.
- **No Unqualified Fire Claims**: Never present satellite thermal hotspots as confirmed active bushfires without explicit sensor qualification. Never generate evacuation advice, predict fire spread trajectories, or declare any location "safe."
- **Persistent Emergency Header**: Render a persistent, high-contrast top safety bar across the web console and in all CLI/report outputs displaying:
  - Explicit notice: *"Research & Historical Information Only — Not for Emergency Decisions. Call 000 in life-threatening emergencies."*
  - Region-aware direct links to official state emergency authorities:
    - **NSW**: [NSW Rural Fire Service (Hazards Near Me)](https://www.google.com/search?q=https%3A%2F%2Fwww.rfs.nsw.gov.au%2F)
    - **VIC**: [VicEmergency](https://www.google.com/search?q=https%3A%2F%2Femergency.vic.gov.au%2F)
    - **QLD**: [QLD Fire Department Current Incidents](https://www.google.com/search?q=https%3A%2F%2Fwww.fire.qld.gov.au%2FCurrent-Incidents)
    - **WA**: [Emergency WA](https://www.google.com/search?q=https%3A%2F%2Fwww.emergency.wa.gov.au%2F)
    - **SA**: [SA Country Fire Service](https://www.google.com/search?q=https%3A%2F%2Fwww.cfs.sa.gov.au%2F)
    - **TAS**: [TasALERT](https://www.google.com/search?q=https%3A%2F%2Falert.tas.gov.au%2F)
    - **ACT**: [ACT Emergency Services Agency](https://www.google.com/search?q=https%3A%2F%2Fesa.act.gov.au%2F)
    - **NT**: [NT Fire Incident Map](https://www.google.com/search?q=https%3A%2F%2Fpfes.nt.gov.au%2Fincidentmap%2F)

## 2. MANDATORY FREE-ONLY & ZERO-LOCK-IN POLICY

Enforce these engineering rules across every C++ translation unit:
- **Zero Paid Dependencies**: Use only open-source C++ libraries (`libcurl`, `sqlite3`, `nlohmann/json`, `cpp-httplib`, `doctest`/`Catch2`) and free public geospatial feeds.
- **Keyless Out-of-the-Box Operation**: The compiled C++ server (`bushfire_memory_server`) and CLI (`bushfire_memory_cli`) must launch and serve working incidents, hotspots, historical fire polygons, WMS severity layer descriptors, and satellite scene comparisons **immediately with zero API keys configured**.
- **Optional Free Key Enhancements**: Support reading an optional `NASA_FIRMS_MAP_KEY` from environment variables or runtime settings (stored locally in SQLite settings table, never hardcoded in C++ source files), falling back automatically to Geoscience Australia DEA Hotspots and verified historical archive fixtures when unconfigured.
- **Respectful Rate-Limiting & SQLite Caching**: Enforce per-provider request deduplication via `std::mutex`, minimum polling intervals (minimum 300 seconds for live feeds), `CURLOPT_TIMEOUT` (12 seconds max), TLS verification (`CURLOPT_SSL_VERIFYPEER = 1L`), and persistent `SQLite3` caching with explicit Time-To-Live (TTL) expiration and bounded row/byte eviction.

## 3. VERIFIED PUBLIC DATA SOURCES & C++ PROVIDER ADAPTERS

Implement a strict C++20 **Provider-Adapter Pattern** inheriting from an abstract base class `IDataProviderAdapter`. Because all external HTTP fetches happen server-side in C++ via `libcurl`, the platform avoids browser CORS blocks entirely while still maintaining a 3-tier resilience strategy (**Tier 1: Live `libcurl` HTTPS Fetch** $\rightarrow$ **Tier 2: Local `SQLite3` Stale Cache** $\rightarrow$ **Tier 3: Embedded Verified Historical Snapshot**).

| Provider ID | Official Source & Licence | Technical Service URL & Format | Role in C++ Engine |
| :--- | :--- | :--- | :--- |
| `nsw-rfs-incidents` | [NSW RFS Current Incidents Feed](https://www.google.com/search?q=https%3A%2F%2Fdata.nsw.gov.au%2Fdata%2Fdataset%2Fnsw-rural-fire-service-current-incidents-feed) (CC BY 4.0) | `https://www.rfs.nsw.gov.au/feeds/majorIncidents.json` ([GeoJSON Resource](https://www.google.com/search?q=https%3A%2F%2Fdata.nsw.gov.au%2Fdata%2Fdataset%2Fnsw-rural-fire-service-current-incidents-feed%2Fresource%2Fc2cb1807-e7ca-4962-851f-c2eeda9e5a69)) | Live reported NSW bushfire incidents, alert levels, geometries, and RFS status metadata parsed in C++. |
| `dea-hotspots` | [Digital Earth Australia (DEA) Hotspots](https://www.google.com/search?q=https%3A%2F%2Fknowledge.dea.ga.gov.au%2Fdata%2Fproduct%2Fdea-hotspots%2F) — Geoscience Australia (CC BY 4.0) | `https://hotspots.dea.ga.gov.au/geoserver/wfs` (WFS GeoJSON) & [DEA Burnt Area Vectors](https://www.google.com/search?q=https%3A%2F%2Fknowledge.dea.ga.gov.au%2Fdata%2Fversion-history%2Fdea-burnt-area-vectors-sentinel-2-near-real-time-1.0.0%2F) | Default keyless Australian satellite thermal hotspot detections and provisional Sentinel-2 burnt area vectors. |
| `nasa-firms` | [NASA FIRMS Active Fire Data](https://www.google.com/search?q=https%3A%2F%2Ffirms.modaps.eosdis.nasa.gov%2Fapi%2Fdata_availability%2F) (NASA Open Data) | `https://firms.modaps.eosdis.nasa.gov/api/area/csv/{MAP_KEY}/{SOURCE}/112,-44,154,-10/{DAYS}` (VIIRS / MODIS CSV) | High-cadence VIIRS (`VIIRS_SNPP_NRT`, `VIIRS_NOAA20_NRT`) and MODIS active fire detections parsed by a zero-copy C++ CSV parser. |
| `nsw-npws-history` | [NPWS Fire History - Wildfires and Prescribed Burns](https://www.google.com/search?q=https%3A%2F%2Fdata.nsw.gov.au%2Fdata%2Fdataset%2Ffire-history-wildfires-and-prescribed-burns-1e8b6) (CC BY 4.0) | [NSW SEED WFS](https://www.google.com/search?q=https%3A%2F%2Fdata.nsw.gov.au%2Fdata%2Fdataset%2Ffire-history-wildfires-and-prescribed-burns-1e8b6%2Fresource%2F071dfa2a-e623-417c-b746-0fedaecff857) / ArcGIS REST GeoJSON (`NPWS_Fire_History/MapServer/0/query`) | Searchable historical vector fire perimeters, burn types (Wildfire vs. Prescribed Burn), fire seasons, and area in hectares. |
| `nsw-fesm-wms` | [NSW Fire Extent and Severity Mapping (FESM)](https://www.google.com/search?q=https%3A%2F%2Fnaturalhazardsscience-hub.seed.nsw.gov.au%2Farticle%2F4-nsw-fire-extent-and-severity-mapping-fesm) (CC BY 4.0) | NSW SEED WMS (`EPSG:3857` raster tile overlay) + [FESM Data Quality Statement](https://www.google.com/search?q=https%3A%2F%2Fwww.data.nsw.gov.au%2Fdata%2Fen%2Fdataset%2Ffire-extent-and-severity-mapping-fesm%2Fresource%2F07176993-4c03-40ac-ad51-9382006526a7) | Validated post-fire canopy and understorey burn severity raster classifications (Unburnt, Low, Moderate, High, Extreme). |
| `nsw-geebam-wms` | [Google Earth Engine Burnt Area Map (GEEBAM)](https://www.google.com/search?q=https%3A%2F%2Fdata.nsw.gov.au%2Fdata%2Fdataset%2Fgoogle-earth-engine-burnt-area-map-geebam) (CC BY 4.0) | NSW SEED WMS (`EPSG:3857` raster tile overlay) | Rapid interim post-fire vegetation impact layer; must display explicit warning that it lacks ground-truthing compared to FESM. |
| `dea-stac-ows` | [DEA STAC Explorer](https://www.google.com/search?q=https%3A%2F%2Fexplorer.sandbox.dea.ga.gov.au%2Fabout) & [DEA OWS Server](https://www.google.com/search?q=https%3A%2F%2Fknowledge.dea.ga.gov.au%2Fdata%2Fproduct%2Fdea-fuel-moisture-content%2F) (CC BY 4.0) | STAC Search: `https://explorer.dea.ga.gov.au/stac/search` ([DEA STAC Guide](https://www.google.com/search?q=https%3A%2F%2Fknowledge.dea.ga.gov.au%2Fguides%2Fsetup%2Fgis%2Fstac%2F)) \| WMS: `https://ows.dea.ga.gov.au/` | Keyless Sentinel-2 / Landsat scene search, [DEA Burnt Area Characteristic Layers](https://www.google.com/search?q=https%3A%2F%2Fknowledge.dea.ga.gov.au%2Fdata%2Fproduct%2Fdea-burnt-area-characteristic-layers-sentinel-2-near-real-time%2F), and time-enabled WMS for pre/post-fire visual and NDVI/dNBR comparisons. |
| `copernicus-stac` | [Copernicus Data Space Ecosystem STAC](https://www.google.com/search?q=https%3A%2F%2Fdocumentation.dataspace.copernicus.eu%2FAPIs%2FSTAC.html) | `https://stac.dataspace.copernicus.eu/v1/search` (`sentinel-2-l2a`) | Supplementary global Sentinel-2 L2A scene metadata, cloud cover percentages, footprints, and quicklook previews. |

### Specific C++ Adapter Implementation Rules
1. **NSW RFS GeoJSON Adapter (`src/providers/RfsIncidentAdapter.cpp`)**:
   - Enforce `https://` on `https://www.rfs.nsw.gov.au/feeds/majorIncidents.json` via `libcurl`.
   - Implement a deterministic C++ string/regex parser that extracts structured key-value fields (`ALERT LEVEL`, `LOCATION`, `COUNCIL AREA`, `STATUS`, `TYPE`, `FIRE`, `SIZE`, `RESPONSIBLE AGENCY`, `UPDATED`) from `properties.description` while stripping all HTML tags (`<br />`) to guarantee zero XSS or unsafe HTML injection.
   - Normalize both `Point` and `GeometryCollection` geometries, computing accurate bounding boxes and centroids in C++.
2. **Hotspot Adapters (`src/providers/HotspotAdapter.cpp`)**:
   - Parse both DEA Hotspots GeoJSON and NASA FIRMS CSV streams (`latitude,longitude,bright_ti4,scan,track,acq_date,acq_time,satellite,instrument,confidence,version,bright_ti5,frp,daynight`).
   - Enforce the semantic distinction that every record is a **Satellite Thermal Anomaly / Hotspot**, never an unverified "Confirmed Fire."
3. **Historical Fire Perimeters & Severity Layers (`src/providers/HistoricalFireAdapter.cpp`)**:
   - Query and index historical fire polygons from `nsw-npws-history` into the local SQLite database for sub-millisecond C++ spatial/attribute filtering and similarity scoring.
   - Expose WMS layer descriptors and provenance caveats distinguishing [GEEBAM](https://www.google.com/search?q=https%3A%2F%2Fdata.nsw.gov.au%2Fdata%2Fdataset%2Fgoogle-earth-engine-burnt-area-map-geebam) (rapid uncalibrated index) from [FESM](https://www.google.com/search?q=https%3A%2F%2Fnaturalhazardsscience-hub.seed.nsw.gov.au%2Farticle%2F4-nsw-fire-extent-and-severity-mapping-fesm) (calibrated machine-learning severity classification).
4. **STAC & Spectral Change Engine (`src/providers/StacImageryAdapter.cpp`)**:
   - Query `https://explorer.dea.ga.gov.au/stac/search` (GET/POST) and `https://stac.dataspace.copernicus.eu/v1/search` by `bbox`, `datetime`, and collection.
   - Provide C++ band math helpers and WMS configuration builders for True Colour RGB (`B04, B03, B02`), SWIR Burn Scar (`B12, B8A, B04`), **NDVI**, and **$\Delta\text{NBR}$** with full scientific caveat metadata.

## 4. C++20 ARCHITECTURE, REST API & EMBEDDED WEB WORKBENCH

Architect the C++ application so that all data ingestion, caching, spatial geometry math, similarity scoring, and report generation execute natively in compiled **C++20**, exposed via:
1. **`bushfire_memory_server`**: A multithreaded C++20 HTTP/JSON server (`cpp-httplib` bound to `0.0.0.0:8080`) that exposes clean REST endpoints and serves a single-page embedded **MapLibre GL JS Geospatial Workbench** (`web/index.html` + `web/app.js` + `web/styles.css`, using vanilla JS + MapLibre CDN with zero npm/Node build steps).
2. **`bushfire_memory_cli`**: A command-line interface supporting headless data ingestion, similarity queries, cache inspection, and GeoJSON/CSV/Markdown report generation directly inside the Docker terminal.
3. **`bushfire_memory_tests`**: A self-contained C++20 test binary verifying every parser, mathematical formula, SQLite cache operation, and security check.

### C++20 REST API Surface (`bushfire_memory_server` on Port `8080`)
- `GET /api/v1/health` — Returns real-time status, HTTP latency (`ms`), `pubDate` vs. `fetchedAt` timestamps, SQLite cache hit/staleness status, and record counts across all 8 providers.
- `GET /api/v1/incidents` — Returns normalized NSW RFS incidents filtered by `bbox`, `alert_level`, `fire_only`, or `search` query.
- `GET /api/v1/hotspots` — Returns DEA and NASA FIRMS thermal hotspots filtered by `bbox`, `min_frp`, `confidence`, and `hours`.
- `GET /api/v1/history` — Returns historical bushfires and prescribed burns filtered by `bbox`, `season`, `fire_type`, `min_area_ha`, and text query.
- `GET /api/v1/similarity?event_id={ID}&limit=5` — Executes the C++20 4-factor **Similar Historical Events** mathematical engine and returns the top matches with individual component scores ($S_{\text{geo}}, S_{\text{size}}, S_{\text{season}}, S_{\text{type}}$) and mandatory non-predictive disclaimers.
- `GET /api/v1/stac/search?bbox={BBOX}&start={ISO}&end={ISO}&max_cloud={PCT}` — Queries DEA / Copernicus STAC via C++ `libcurl` and returns normalized scene metadata plus pre/post-fire DEA OWS WMS URLs.
- `GET /api/v1/layers/wms` — Returns verified WMS layer definitions, legends, and quality statements for FESM, GEEBAM, and DEA Spectral Indices.
- `POST /api/v1/settings` — Updates runtime settings (such as optional `NASA_FIRMS_MAP_KEY` or force-refresh trigger) in SQLite.
- `GET /api/v1/export/{format}` (`geojson` | `csv` | `markdown`) — Generates and streams a downloadable research export directly from C++.

### Deterministic C++20 Mathematical Formulas (`src/engine/SimilarityEngine.cpp`)
Implement the 4-factor historical similarity scorer in `double SimilarityEngine::computeScore(const HistoricalFireRecord& a, const HistoricalFireRecord& b)`:
$$\text{Similarity}(A, B) = 100.0 \times \left( 0.35 \cdot S_{\text{geo}} + 0.30 \cdot S_{\text{size}} + 0.20 \cdot S_{\text{season}} + 0.15 \cdot S_{\text{type}} \right)$$
- **Geographic Proximity ($S_{\text{geo}}$)**: $\exp(-d_{\text{km}} / 250.0)$, where $d_{\text{km}}$ is the great-circle Haversine distance in kilometres between centroids.
- **Log-Area Scale Similarity ($S_{\text{size}}$)**: $\max\left(0.0, 1.0 - \frac{|\ln(\text{Area}_A + 1.0) - \ln(\text{Area}_B + 1.0)|}{6.0}\right)$.
- **Seasonal Circular Similarity ($S_{\text{season}}$)**: $1.0 - \frac{\min(|\text{DOY}_A - \text{DOY}_B|, 365 - |\text{DOY}_A - \text{DOY}_B|)}{182.5}$, where $\text{DOY} \in [1, 365]$.
- **Fire Category & Bioregion Match ($S_{\text{type}}$)**: $1.0$ if both share `fireType` and `bioregionOrCouncil`, $0.5$ if same `fireType` only, $0.0$ otherwise.

## 5. CANONICAL C++20 DATA MODEL & PROJECT STRUCTURE

### Canonical C++20 Header (`include/bushfire/Types.hpp`)
```cpp
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace bushfire {

enum class FreshnessStatus {
    Live,
    CachedFresh,
    StaleCache,
    FallbackSnapshot,
    Error
};

struct SourceProvenance {
    std::string providerId;
    std::string providerName;
    std::string datasetTitle;
    std::string sourceUrl;
    std::string documentationUrl;
    std::string licence;
    std::optional<std::string> sourcePublishedAtIso;
    std::string fetchedAtIso;
    FreshnessStatus freshness;
    std::string caveatNote;
};

struct DataSourceStatus {
    std::string providerId;
    std::string name;
    std::string endpointUrl;
    std::string docsUrl;
    std::string licence;
    FreshnessStatus status;
    std::optional<std::string> lastSuccessAtIso;
    std::optional<std::string> sourceUpdatedAtIso;
    std::optional<long> latencyMs;
    std::size_t recordCount{0};
    std::optional<std::string> errorMessage;
    bool requiresKey{false};
    bool keyConfigured{false};
};

struct UnifiedIncident {
    std::string id;
    std::string originalId;
    std::string title;
    std::string alertLevel; // Emergency Warning, Watch and Act, Advice, Not Applicable
    std::string status;
    std::string category;
    std::string councilArea;
    std::string responsibleAgency;
    std::optional<double> sizeHa;
    bool isFire{false};
    double centroidLng{0.0};
    double centroidLat{0.0};
    nlohmann::json geometryJson;
    std::optional<std::string> updatedAtIso;
    std::optional<std::string> updatedAtRaw;
    SourceProvenance provenance;
};

struct HotspotDetection {
    std::string id;
    double latitude{0.0};
    double longitude{0.0};
    std::string acquiredAtIso;
    std::string satellite;
    std::string instrument;
    std::string confidence;
    std::optional<double> frpMw;
    std::optional<double> brightnessK;
    std::string dayNight;
    SourceProvenance provenance;
};

struct HistoricalFireRecord {
    std::string id;
    std::string fireName;
    std::string fireSeason;
    std::optional<std::string> ignitionDateIso;
    std::optional<std::string> extinguishedDateIso;
    std::string fireType; // Wildfire, Prescribed Burn, Other
    double areaHa{0.0};
    std::string bioregionOrCouncil;
    int dayOfYear{1};
    double centroidLng{0.0};
    double centroidLat{0.0};
    std::array<double, 4> bbox{0.0, 0.0, 0.0, 0.0};
    nlohmann::json geometryJson;
    SourceProvenance provenance;
};

struct SimilarityMatch {
    HistoricalFireRecord record;
    double totalScorePct{0.0};
    double geoScore{0.0};
    double sizeScore{0.0};
    double seasonScore{0.0};
    double typeScore{0.0};
    double distanceKm{0.0};
    std::string explanation;
};

struct SatelliteSceneItem {
    std::string id;
    std::string collection;
    std::string platform;
    std::string acquiredAtIso;
    std::optional<double> cloudCoverPct;
    std::array<double, 4> bbox{0.0, 0.0, 0.0, 0.0};
    nlohmann::json geometryJson;
    std::optional<std::string> previewUrl;
    std::string wmsLayerName;
    std::string stacSelfUrl;
    double gsdMeters{10.0};
    SourceProvenance provenance;
};

} // namespace bushfire
```

### Target Modular C++20 Repository Layout
1. `CMakeLists.txt` — C++20 build configuration compiling `bushfire_core` (static library), `bushfire_memory_server`, `bushfire_memory_cli`, and `bushfire_memory_tests`.
2. `Dockerfile` & `docker-compose.yml` — Multi-stage Linux Docker build (`ubuntu:24.04` with `g++`, `cmake`, `ninja-build`, `libcurl4-openssl-dev`, `libsqlite3-dev`, `nlohmann-json3-dev`, `libhttplib-dev`).
3. `include/bushfire/Types.hpp` — Canonical C++20 structs and JSON serialization declarations.
4. `include/bushfire/GeoUtils.hpp` & `src/core/GeoUtils.cpp` — Haversine great-circle distance, bounding-box filtering, polygon centroid calculator, Australian dual-timezone (`AEST`/`AEDT` + `UTC`) ISO 8601 formatter, and RFS HTML description key-value parser.
5. `include/bushfire/CacheStore.hpp` & `src/core/CacheStore.cpp` — Thread-safe `SQLite3` persistent cache (`CREATE TABLE IF NOT EXISTS http_cache ...`) with TTL, staleness detection, and bounded size eviction.
6. `include/bushfire/HttpClient.hpp` & `src/core/HttpClient.cpp` — RAII `libcurl` HTTPS wrapper with timeout, user-agent, conditional ETag/If-Modified-Since headers, and latency measurement.
7. `include/bushfire/VerifiedSnapshots.hpp` & `src/data/VerifiedSnapshots.cpp` — Embedded authentic historical datasets (Gospers Mountain 2019–20, Currowan 2019–20, Dunns Road 2020, Green Wattle Creek 2019–20, Warrumbungle 2013, Brindabella/Canberra 2003, NSW RFS GeoJSON samples, DEA/FIRMS hotspots, and DEA STAC Sentinel-2 scenes) for guaranteed offline/fallback operation.
8. `src/providers/RfsIncidentAdapter.cpp` — NSW RFS Current Incidents GeoJSON adapter.
9. `src/providers/HotspotAdapter.cpp` — DEA Hotspots WFS + NASA FIRMS CSV C++ parser.
10. `src/providers/HistoricalFireAdapter.cpp` — NSW NPWS Fire History adapter + FESM & GEEBAM WMS layer registry.
11. `src/providers/StacImageryAdapter.cpp` — DEA STAC (`explorer.dea.ga.gov.au/stac/search`) & Copernicus STAC adapter + spectral index metadata (RGB, SWIR, NDVI, $\Delta\text{NBR}$).
12. `src/engine/SimilarityEngine.cpp` — 4-factor historical similarity scoring engine.
13. `src/engine/ExportService.cpp` — RFC 7946 GeoJSON, RFC 4180 CSV, and Markdown research briefing generator.
14. `src/server/HttpServer.cpp` & `src/main_server.cpp` — `cpp-httplib` REST server serving `/api/v1/*` and static files from `web/`.
15. `src/main_cli.cpp` — Headless CLI tool (`--status`, `--incidents`, `--similar`, `--export-report`).
16. `web/index.html`, `web/app.js`, `web/styles.css` — Zero-build embedded MapLibre GL JS 4-workspace frontend (Map Workbench, Incident & History Explorer, Satellite & Change Inspector, Data Health & Exports) with accessible non-colour-only symbology and persistent emergency header.
17. `tests/test_main.cpp` — Comprehensive C++20 unit & certification test suite.

## 6. LINUX DOCKER SANDBOX & GITHUB WORKFLOW

Follow this exact operational sequence:
1. **Scaffold C++20 Codebase**: Create the complete directory structure (`include/`, `src/`, `web/`, `tests/`, `CMakeLists.txt`, `Dockerfile`, `.gitignore`, `README.md`).
2. **Compile & Test in Linux Docker Sandbox**:
   - Do **not** compile on the local Windows machine.
   - Start/resume the Linux Docker container on `https://agentic-platform.docker.com/sandboxes` (or execute inside the Linux Docker container environment).
   - Run:
     ```bash
     cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
     cmake --build build --parallel
     ./build/bushfire_memory_tests
     ./build/bushfire_memory_cli --self-check
     ```
   - Verify zero compiler warnings/errors and 100% passing certification assertions.
   - **Pause the container immediately** once compilation and verification tests complete when not actively serving requests.
3. **Push to GitHub**:
   - Verify `.gitignore` excludes `build/`, `*.sqlite3`, `.env`, and object binaries.
   - Commit all C++20 sources, headers, CMake files, Docker configurations, embedded web assets, and `CERTIFICATION_REPORT.md` and push to `https://github.com/vishalvermauts/Bushfire-Memory.git`.