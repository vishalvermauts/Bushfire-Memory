# Provider Verification & Endpoint Dossier

**Platform**: Bushfire Memory (v3.0 C++20 Edition)  
**Date of Verification**: 2026-10-20  
**Verification Method**: Automated `curl` HTTPS HEAD/GET requests, TLS handshake inspection, response header analysis, and schema validation.

---

## 1. NSW Rural Fire Service — Major Incidents Feed
* **Provider Identifier**: `nsw-rfs-incidents`
* **Official Publisher**: NSW Rural Fire Service (RFS), Government of New South Wales
* **Dataset Title**: NSW RFS Current Incidents Feed (GeoJSON)
* **Licence**: Creative Commons Attribution 4.0 International (CC BY 4.0)
* **Official Documentation / Portal**: [data.nsw.gov.au/data/dataset/nsw-rural-fire-service-current-incidents-feed](https://data.nsw.gov.au/data/dataset/nsw-rural-fire-service-current-incidents-feed)
* **Verified Technical Endpoint**: `https://www.rfs.nsw.gov.au/feeds/majorIncidents.json`
* **Authentication**: None (Open Public Access)
* **Transport Protocol**: HTTPS / TLS 1.3
* **CORS Behavior**: Does NOT supply `Access-Control-Allow-Origin: *` in direct browser fetches (served via Cloudflare with strict anti-bot and CDN caching). Requires server-side ingestion via `libcurl`.
* **Format**: GeoJSON FeatureCollection (`EPSG:4326`)
* **Geometry Types**: `Point` (centroid) and `GeometryCollection` (containing both Point and Polygon rings for mapped perimeter boundaries).
* **Live Verification Evidence (Observed Response)**:
  * HTTP Status: `200 OK`
  * Content-Type: `application/json`
  * Cache-Control: `max-age=30, must-revalidate`
* **Key Fields Extracted via C++ Parser**:
  * `properties.title`: Fire name
  * `properties.category`: Alert level (`Emergency Warning`, `Watch and Act`, `Advice`, `Not Applicable`)
  * `properties.description`: Key-value HTML string parsed into structured fields (`ALERT LEVEL`, `LOCATION`, `COUNCIL AREA`, `STATUS`, `TYPE`, `FIRE`, `SIZE`, `RESPONSIBLE AGENCY`, `UPDATED`). HTML tags (`<br />`) are stripped cleanly without DOM injection.

---

## 2. Digital Earth Australia (Geoscience Australia) — Hotspots WFS
* **Provider Identifier**: `dea-hotspots`
* **Official Publisher**: Geoscience Australia, Australian Government
* **Dataset Title**: DEA Hotspots (Sentinel-3, NOAA-20, Suomi-NPP VIIRS, Terra/Aqua MODIS)
* **Licence**: Creative Commons Attribution 4.0 International (CC BY 4.0)
* **Official Documentation**: [knowledge.dea.ga.gov.au/data/product/dea-hotspots](https://knowledge.dea.ga.gov.au/data/product/dea-hotspots/)
* **Verified Technical Endpoint**:
  * WFS Capabilities: `https://hotspots.dea.ga.gov.au/geoserver/wfs?service=WFS&version=1.1.0&request=GetCapabilities`
  * WFS Feature Query: `https://hotspots.dea.ga.gov.au/geoserver/wfs?service=WFS&version=1.1.0&request=GetFeature&typeName=public:hotspots&outputFormat=application/json`
* **Authentication**: None (Open Public Access)
* **Live Verification Evidence (Observed Response)**:
  * HTTP Status: `200 OK`
  * Content-Type: `application/xml` (Capabilities) / `application/json` (Features)
  * Server: Jetty on Amazon CloudFront
* **Key Attributes**: Latitude, longitude, satellite, sensor, confidence (`low`, `nominal`, `high`), FRP (`MW`), brightness temp (`K`), day/night flag.
* **Semantic Rule**: Records are classified as **Satellite Thermal Anomalies / Hotspots**, never unverified active bushfires.

---

## 3. NASA FIRMS (Fire Information for Resource Management System)
* **Provider Identifier**: `nasa-firms`
* **Official Publisher**: NASA Earth Observing System Data and Information System (EOSDIS)
* **Dataset Title**: VIIRS / MODIS Active Fire Detections
* **Licence**: NASA Open Data Policy (No copyright restrictions)
* **Official Documentation**: [firms.modaps.eosdis.nasa.gov/api/data_availability](https://firms.modaps.eosdis.nasa.gov/api/data_availability/)
* **Technical Endpoint**:
  * Keyless public query fallback / Area CSV: `https://firms.modaps.eosdis.nasa.gov/api/area/csv/{MAP_KEY}/{SOURCE}/{BBOX}/{DAYS}`
  * Australia Bounding Box: `112,-44,154,-10`
* **Authentication**: Free registration key (`MAP_KEY`) via EOSDIS. When no key is configured, the system falls back seamlessly to DEA Hotspots and verified historical snapshots.
* **Format**: Comma-Separated Values (CSV) parsed in C++ via a memory-efficient zero-copy streaming parser.

---

## 4. NSW National Parks and Wildlife Service (NPWS) — Fire History
* **Provider Identifier**: `nsw-npws-history`
* **Official Publisher**: Department of Climate Change, Energy, the Environment and Water (NSW DCCEEW)
* **Dataset Title**: NPWS Fire History - Wildfires and Prescribed Burns
* **Licence**: Creative Commons Attribution 4.0 International (CC BY 4.0)
* **Official Documentation / Portal**: [data.nsw.gov.au/data/dataset/fire-history-wildfires-and-prescribed-burns-1e8b6](https://data.nsw.gov.au/data/dataset/fire-history-wildfires-and-prescribed-burns-1e8b6)
* **Verified Technical Service**: NSW SEED ArcGIS REST / WFS Services
* **Format**: GeoJSON FeatureCollection (`EPSG:4326` perimeters)
* **Key Attributes**: Fire name, fire season (e.g., `2019-20`), ignition date, extinguished date, fire type (`Wildfire` vs. `Prescribed Burn`), area in hectares (`Area_ha`), council/bioregion.

---

## 5. NSW Fire Extent and Severity Mapping (FESM)
* **Provider Identifier**: `nsw-fesm-wms`
* **Official Publisher**: NSW Department of Planning and Environment (DPE)
* **Dataset Title**: NSW Fire Extent and Severity Mapping (FESM)
* **Licence**: Creative Commons Attribution 4.0 International (CC BY 4.0)
* **Documentation**: [naturalhazardsscience-hub.seed.nsw.gov.au/article/4-nsw-fire-extent-and-severity-mapping-fesm](https://naturalhazardsscience-hub.seed.nsw.gov.au/article/4-nsw-fire-extent-and-severity-mapping-fesm)
* **Service Type**: WMS Raster Tile Overlay (`EPSG:3857`, PNG tiles)
* **Scientific Caveat**: Multi-class validated machine-learning classification (Unburnt, Low Severity, Moderate Severity, High Severity, Extreme Severity) calibrated with post-fire field validation.

---

## 6. Google Earth Engine Burnt Area Map (GEEBAM)
* **Provider Identifier**: `nsw-geebam-wms`
* **Official Publisher**: NSW Department of Planning, Industry and Environment
* **Dataset Title**: Google Earth Engine Burnt Area Map (GEEBAM)
* **Licence**: Creative Commons Attribution 4.0 International (CC BY 4.0)
* **Documentation**: [data.nsw.gov.au/data/dataset/google-earth-engine-burnt-area-map-geebam](https://data.nsw.gov.au/data/dataset/google-earth-engine-burnt-area-map-geebam)
* **Service Type**: WMS Raster Tile Overlay (`EPSG:3857`, PNG tiles)
* **Scientific Caveat**: Rapid, uncalibrated Sentinel-2 dNBR threshold product created during the 2019–20 emergency for rapid assessment. The UI explicitly alerts users that GEEBAM lacks the field calibration and canopy layer distinction of FESM.

---

## 7. Digital Earth Australia (DEA) STAC & OWS
* **Provider Identifier**: `dea-stac-ows`
* **Official Publisher**: Geoscience Australia
* **Dataset Title**: DEA Sentinel-2 Analysis Ready Data (ARD) & WMS Services
* **Licence**: Creative Commons Attribution 4.0 International (CC BY 4.0)
* **Documentation**: [explorer.dea.ga.gov.au/about](https://explorer.dea.ga.gov.au/about) & [knowledge.dea.ga.gov.au](https://knowledge.dea.ga.gov.au/)
* **Verified Technical Endpoints**:
  * STAC API: `https://explorer.dea.ga.gov.au/stac/search` (`HTTP 200 OK` verified)
  * OWS WMS: `https://ows.dea.ga.gov.au/`
* **Authentication**: None (Open Keyless Access)
* **Supported Operations**: Bounding box search, datetime range filtering, cloud cover thresholding, and time-enabled WMS layer fetching for spectral composite comparison.

---

## 8. Copernicus Data Space Ecosystem STAC
* **Provider Identifier**: `copernicus-stac`
* **Official Publisher**: European Space Agency (ESA) / European Commission
* **Dataset Title**: Sentinel-2 Level-2A Surface Reflectance
* **Licence**: Copernicus Open Access Policy
* **Documentation**: [documentation.dataspace.copernicus.eu/APIs/STAC.html](https://documentation.dataspace.copernicus.eu/APIs/STAC.html)
* **Technical Endpoint**: `https://stac.dataspace.copernicus.eu/v1/search`
* **Supported Operations**: Global Sentinel-2 L2A scene metadata, cloud cover percentage, and granule preview links.
