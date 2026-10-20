# Bushfire Memory (v3.0 C++20 Edition)
## Australian Bushfire Intelligence, Satellite Hotspot Analysis & Historical Exploration Platform

> [!IMPORTANT]
> **CRITICAL PUBLIC SAFETY NOTICE: RESEARCH & HISTORICAL OBSERVATION ONLY**  
> Bushfire Memory is an observational research platform and historical intelligence workbench. It is **NOT** an emergency warning service, evacuation router, or emergency dispatch tool.  
> In life-threatening emergencies, immediately dial **000** or consult official state emergency services:
> - **NSW**: [NSW Rural Fire Service](https://www.rfs.nsw.gov.au/) (Hazards Near Me)
> - **VIC**: [VicEmergency](https://emergency.vic.gov.au/)
> - **QLD**: [Queensland Fire Department Current Incidents](https://www.fire.qld.gov.au/Current-Incidents)
> - **WA**: [Emergency WA](https://www.emergency.wa.gov.au/)
> - **SA**: [Country Fire Service](https://www.cfs.sa.gov.au/)
> - **TAS**: [TasALERT](https://alert.tas.gov.au/)
> - **ACT**: [ACT Emergency Services Agency](https://esa.act.gov.au/)
> - **NT**: [NT Fire Incident Map](https://pfes.nt.gov.au/incidentmap)

---

## 1. Architectural Highlights (100% C++20 + Zero-Build Frontend)

* **Native C++20 Core**: Data ingestion, persistent caching, geospatial geometric analysis, 4-factor similarity scoring, and report generation execute natively in compiled C++20 using `CMake`, `libcurl`, `SQLite3`, and `cpp-httplib`.
* **Zero Node.js / Zero-Build Frontend**: The web console uses pure semantic HTML5, CSS3, and vanilla JavaScript loading MapLibre GL JS from an open CDN, served directly by the embedded C++ HTTP server.
* **Resilient 3-Tier Ingestion**:
  1. **Tier 1 (Live Fetch)**: Server-side `libcurl` HTTPS ingestion with strict timeouts and ETag caching.
  2. **Tier 2 (SQLite3 Cache)**: Thread-safe persistent cache with TTL expiration and staleness tracking.
  3. **Tier 3 (Verified Historical Snapshot)**: Authentic historical archives (Black Summer 2019–20: Gospers Mountain, Currowan, Dunns Road, Green Wattle Creek; Warrumbungle 2013; Canberra 2003) guaranteeing immediate out-of-the-box operation with zero API keys required.
* **100% Free & Open-Data**: No paid Google Maps APIs, Mapbox tokens, or Firebase services required.

---

## 2. Four Deep Workspaces

1. **Interactive Fire & Intelligence Map (MapLibre GL JS)**:
   - High-contrast accessible symbology (geometric badges and text indicators: `EMG`, `W&A`, `ADV`, `HOT`, `HIST`).
   - Thermal hotspots scaled by Fire Radiative Power (MW) and sensor confidence rings.
   - Vector historical fire perimeters (amber fill for wildfires, teal fill for prescribed burns).
   - Toggleable WMS overlays for NSW Fire Extent and Severity Mapping (FESM) and GEEBAM with opacity slider.
   - Click-to-inspect feature provenance drawer.
2. **Incident & History Explorer + Similarity Engine**:
   - Filter and search active incidents and historical fire perimeters.
   - Deterministic 4-factor historical similarity calculator ($S_{\text{geo}}$, $S_{\text{size}}$, $S_{\text{season}}$, $S_{\text{type}}$) with explainable score breakdown charts and non-predictive research disclaimers.
3. **Satellite & Change Inspector**:
   - Query Digital Earth Australia (DEA) and Copernicus Sentinel-2 Level-2A Analysis Ready Data (ARD).
   - Split / curtain swipe comparator over documented burn scar zones.
   - Optical mode selector: True Colour RGB, False Colour SWIR (Burn Scar), NDVI, and dNBR.
   - Scientific caveats disclosure card.
4. **Data Health, Settings & Reproducible Exports**:
   - Live provider latency and status matrix.
   - Runtime configuration for optional NASA FIRMS `MAP_KEY`.
   - In-app System Self-Test suite.
   - Downloadable exports: RFC 7946 GeoJSON, tabular CSVs, and Markdown research briefings.

---

## 3. Quick Start with Docker (Recommended)

To compile and run inside an authorized Linux container:

```bash
# Build multi-stage Docker image (compiles C++20 and runs test assertions)
docker build -t bushfire-memory .

# Run container on port 8080
docker run -p 8080:8080 bushfire-memory
```

Access the web workbench at `http://localhost:8080`.

---

## 4. Native Linux / Container Build with CMake

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install -y build-essential cmake ninja-build libcurl4-openssl-dev libsqlite3-dev

# Configure and compile with Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2

# Run automated test suite
./build/bushfire_memory_tests

# Run CLI self-check
./build/bushfire_memory_cli --self-check

# Start C++ HTTP server
./build/bushfire_memory_server --host 127.0.0.1 --port 8080 --web web
```

---

## 5. C++20 REST API Surface

| Endpoint | Method | Description |
| :--- | :--- | :--- |
| `/api/v1/health` | `GET` | Real-time status, latency, cache health, and record counts across all data providers. |
| `/api/v1/incidents` | `GET` | Normalized NSW RFS reported incidents (supports `bbox`, `alert_level`, `fire_only`, `search`). |
| `/api/v1/hotspots` | `GET` | Satellite thermal anomalies from DEA Hotspots & NASA FIRMS (supports `bbox`, `min_frp`, `confidence`, `hours`). |
| `/api/v1/history` | `GET` | Historical vector fire perimeters (supports `bbox`, `season`, `fire_type`, `min_area_ha`, `search`). |
| `/api/v1/similarity` | `GET` | 4-factor deterministic historical similarity search (`event_id`, `limit`). |
| `/api/v1/stac/search` | `GET` | Query DEA / Copernicus STAC catalogue for Sentinel-2 ARD scenes. |
| `/api/v1/layers/wms` | `GET` | Verified WMS layer descriptors for FESM, GEEBAM, and DEA Burnt Area overlays. |
| `/api/v1/spectral/modes` | `GET` | Spectral band formulas, wavelengths, and scientific caveat metadata. |
| `/api/v1/settings` | `POST` | Update runtime settings (`NASA_FIRMS_MAP_KEY`, `force_refresh`, `clear_cache`). |
| `/api/v1/export/{fmt}` | `GET` | Stream research exports (`geojson`, `csv_incidents`, `csv_history`, `csv_hotspots`, `markdown`). |

---

## 6. Official Data Sources & Licences

* **NSW Rural Fire Service (RFS)**: Major Incidents Feed (CC BY 4.0)
* **Geoscience Australia**: Digital Earth Australia (DEA) Hotspots, Burnt Area Characteristics, Sentinel-2 ARD (CC BY 4.0)
* **NASA EOSDIS**: Fire Information for Resource Management System (FIRMS) (NASA Open Data)
* **NSW Department of Climate Change, Energy, the Environment and Water**: NPWS Fire History, FESM v1, GEEBAM (CC BY 4.0)
* **European Space Agency / European Commission**: Copernicus Data Space Ecosystem (Open Access)

---

## 7. Hosting on Vercel

Bushfire Memory supports **1-click deployment on Vercel**:

* **Standalone Edge Mode (100% Free Tier)**: Serves the zero-build MapLibre console with pre-rendered data snapshots and browser-executed 4-factor deterministic similarity scoring.
* **Hybrid Mode**: Deploys the static frontend on Vercel's global CDN while reverse-proxying `/api/*` to the C++20 container (Fly.io / Railway / Docker) using `vercel.json` rewrites.

See [`docs/VERCEL_DEPLOYMENT.md`](file:///c:/AI/Bushfire%20Memory/docs/VERCEL_DEPLOYMENT.md) for full instructions.


