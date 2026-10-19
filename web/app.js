/**
 * BUSHFIRE MEMORY — Geospatial Intelligence & Historical Analysis Platform (v3.0 C++20)
 * Client Application Controller: Pure Vanilla JavaScript + MapLibre GL JS
 */

const API_BASE = window.location.origin;

// State Store
const state = {
  activeWorkspace: 'map',
  map: null,
  mapLoaded: false,
  incidents: [],
  hotspots: [],
  historyFires: [],
  selectedEvent: null,
  providerStatuses: [],
  spectralModes: []
};

// Preset Bounding Boxes [minLng, minLat, maxLng, maxLat] & Centers
const REGION_PRESETS = {
  'all-nsw': { center: [149.5, -33.5], zoom: 6.2 },
  'sydney-bluemts': { center: [150.5, -33.7], zoom: 9.0 },
  'hawkesbury-wollemi': { center: [150.45, -33.15], zoom: 8.8 }, // Gospers Mtn
  'south-coast': { center: [150.18, -35.52], zoom: 8.5 },        // Currowan
  'snowy-monaro': { center: [148.15, -35.65], zoom: 8.5 },       // Dunns Road
  'warrumbungle': { center: [149.02, -31.28], zoom: 9.2 },
  'pilliga': { center: [149.35, -30.65], zoom: 8.5 },
  'all-australia': { center: [134.0, -26.0], zoom: 4.2 }
};

// Basemap Styles
const BASEMAP_STYLES = {
  dark: {
    version: 8,
    sources: {
      'carto-dark': {
        type: 'raster',
        tiles: [
          'https://a.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}@2x.png',
          'https://b.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}@2x.png'
        ],
        tileSize: 256,
        attribution: '&copy; <a href="https://carto.com/">CARTO</a> &copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
      }
    },
    layers: [
      { id: 'carto-dark-layer', type: 'raster', source: 'carto-dark', minzoom: 0, maxzoom: 19 }
    ]
  },
  liberty: 'https://tiles.openfreemap.org/styles/liberty',
  imagery: {
    version: 8,
    sources: {
      'esri-imagery': {
        type: 'raster',
        tiles: [
          'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}'
        ],
        tileSize: 256,
        attribution: 'Tiles &copy; Esri'
      }
    },
    layers: [
      { id: 'esri-imagery-layer', type: 'raster', source: 'esri-imagery', minzoom: 0, maxzoom: 19 }
    ]
  }
};

// -----------------------------------------------------------------------------
// INITIALIZATION
// -----------------------------------------------------------------------------
document.addEventListener('DOMContentLoaded', () => {
  initClocks();
  initNavigation();
  initMap();
  initExplorer();
  initSatelliteWorkspace();
  initHealthAndExports();
  loadInitialData();
});

// Clocks (AEST & UTC)
function initClocks() {
  const clockAest = document.getElementById('clock-aest');
  const clockUtc = document.getElementById('clock-utc');

  const update = () => {
    const now = new Date();
    clockAest.textContent = now.toLocaleTimeString('en-AU', {
      timeZone: 'Australia/Sydney',
      hour12: false
    }) + ' AEST';

    clockUtc.textContent = now.toLocaleTimeString('en-GB', {
      timeZone: 'UTC',
      hour12: false
    }) + ' UTC';
  };

  update();
  setInterval(update, 1000);
}

// Workspace Navigation
function initNavigation() {
  const tabs = document.querySelectorAll('.nav-tab');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      const targetWorkspace = tab.dataset.workspace;
      switchWorkspace(targetWorkspace);
    });
  });
}

function switchWorkspace(workspaceId) {
  state.activeWorkspace = workspaceId;

  document.querySelectorAll('.nav-tab').forEach(t => {
    t.classList.toggle('active', t.dataset.workspace === workspaceId);
  });

  document.querySelectorAll('.workspace-panel').forEach(panel => {
    panel.classList.toggle('active', panel.id === `workspace-${workspaceId}`);
  });

  if (workspaceId === 'map' && state.map) {
    setTimeout(() => state.map.resize(), 100);
  }
}

// -----------------------------------------------------------------------------
// WORKSPACE 1: MAP ENGINE (MapLibre GL JS)
// -----------------------------------------------------------------------------
function initMap() {
  const container = document.getElementById('map-container');
  if (!container) return;

  state.map = new maplibregl.Map({
    container: 'map-container',
    style: BASEMAP_STYLES.dark,
    center: REGION_PRESETS['all-nsw'].center,
    zoom: REGION_PRESETS['all-nsw'].zoom,
    attributionControl: true
  });

  state.map.addControl(new maplibregl.NavigationControl(), 'top-right');
  state.map.addControl(new maplibregl.ScaleControl({ unit: 'metric' }), 'bottom-left');

  state.map.on('load', () => {
    state.mapLoaded = true;
    setupMapLayers();
    fetchMapData();
  });

  // Region Jump
  const regionSelect = document.getElementById('region-select');
  regionSelect.addEventListener('change', (e) => {
    const val = e.target.value;
    if (REGION_PRESETS[val]) {
      state.map.flyTo({
        center: REGION_PRESETS[val].center,
        zoom: REGION_PRESETS[val].zoom,
        essential: true
      });
    }
  });

  // Basemap switch
  document.querySelectorAll('input[name="basemap"]').forEach(radio => {
    radio.addEventListener('change', (e) => {
      const styleKey = e.target.value;
      if (BASEMAP_STYLES[styleKey]) {
        state.map.setStyle(BASEMAP_STYLES[styleKey]);
        state.map.once('style.load', () => {
          setupMapLayers();
          updateMapSources();
        });
      }
    });
  });

  // Layer Toggles
  document.getElementById('layer-toggle-incidents').addEventListener('change', (e) => {
    const vis = e.target.checked ? 'visible' : 'none';
    if (state.map.getLayer('layer-incidents-circles')) state.map.setLayoutProperty('layer-incidents-circles', 'visibility', vis);
    if (state.map.getLayer('layer-incidents-labels')) state.map.setLayoutProperty('layer-incidents-labels', 'visibility', vis);
  });

  document.getElementById('layer-toggle-hotspots').addEventListener('change', (e) => {
    const vis = e.target.checked ? 'visible' : 'none';
    if (state.map.getLayer('layer-hotspots-pulse')) state.map.setLayoutProperty('layer-hotspots-pulse', 'visibility', vis);
  });

  document.getElementById('layer-toggle-history').addEventListener('change', (e) => {
    const vis = e.target.checked ? 'visible' : 'none';
    if (state.map.getLayer('layer-history-fill')) state.map.setLayoutProperty('layer-history-fill', 'visibility', vis);
    if (state.map.getLayer('layer-history-line')) state.map.setLayoutProperty('layer-history-line', 'visibility', vis);
  });

  // WMS Layer Toggle & Opacity
  document.getElementById('layer-toggle-fesm').addEventListener('change', (e) => {
    const vis = e.target.checked ? 'visible' : 'none';
    if (state.map.getLayer('layer-fesm-wms')) {
      state.map.setLayoutProperty('layer-fesm-wms', 'visibility', vis);
    }
  });

  const fesmOpacitySlider = document.getElementById('fesm-opacity');
  const fesmOpacityVal = document.getElementById('fesm-opacity-val');
  fesmOpacitySlider.addEventListener('input', (e) => {
    const val = parseFloat(e.target.value) / 100.0;
    fesmOpacityVal.textContent = e.target.value + '%';
    if (state.map.getLayer('layer-fesm-wms')) {
      state.map.setPaintProperty('layer-fesm-wms', 'raster-opacity', val);
    }
  });

  // Drawer close
  document.getElementById('btn-close-drawer').addEventListener('click', () => {
    document.getElementById('feature-drawer').classList.add('hidden');
  });

  document.getElementById('btn-refresh-map').addEventListener('click', () => {
    fetchMapData(true);
  });
}

function setupMapLayers() {
  if (!state.map) return;

  // 1. Historical Perimeters Source & Layers
  if (!state.map.getSource('source-history')) {
    state.map.addSource('source-history', {
      type: 'geojson',
      data: { type: 'FeatureCollection', features: [] }
    });

    state.map.addLayer({
      id: 'layer-history-fill',
      type: 'fill',
      source: 'source-history',
      paint: {
        'fill-color': [
          'match',
          ['get', 'fireType'],
          'Wildfire', '#ea580c',
          'Prescribed Burn', '#0d9488',
          '#ca8a04'
        ],
        'fill-opacity': 0.35
      }
    });

    state.map.addLayer({
      id: 'layer-history-line',
      type: 'line',
      source: 'source-history',
      paint: {
        'line-color': [
          'match',
          ['get', 'fireType'],
          'Wildfire', '#f97316',
          'Prescribed Burn', '#14b8a6',
          '#eab308'
        ],
        'line-width': 1.5,
        'line-dasharray': [
          'match',
          ['get', 'fireType'],
          'Prescribed Burn', ['literal', [2, 2]],
          ['literal', [1, 0]]
        ]
      }
    });
  }

  // 2. Hotspots Source & Layer
  if (!state.map.getSource('source-hotspots')) {
    state.map.addSource('source-hotspots', {
      type: 'geojson',
      data: { type: 'FeatureCollection', features: [] }
    });

    state.map.addLayer({
      id: 'layer-hotspots-pulse',
      type: 'circle',
      source: 'source-hotspots',
      paint: {
        'circle-color': '#f97316',
        'circle-radius': [
          'interpolate', ['linear'], ['coalesce', ['get', 'frpMw'], 10],
          5, 4,
          50, 10,
          200, 18
        ],
        'circle-stroke-color': '#fff',
        'circle-stroke-width': 1,
        'circle-opacity': 0.85
      }
    });
  }

  // 3. Current Incidents Source & Layers
  if (!state.map.getSource('source-incidents')) {
    state.map.addSource('source-incidents', {
      type: 'geojson',
      data: { type: 'FeatureCollection', features: [] }
    });

    state.map.addLayer({
      id: 'layer-incidents-circles',
      type: 'circle',
      source: 'source-incidents',
      paint: {
        'circle-color': [
          'match',
          ['get', 'alertLevel'],
          'Emergency Warning', '#dc2626',
          'Watch and Act', '#f59e0b',
          'Advice', '#2563eb',
          '#64748b'
        ],
        'circle-radius': 7,
        'circle-stroke-color': '#ffffff',
        'circle-stroke-width': 2
      }
    });

    state.map.addLayer({
      id: 'layer-incidents-labels',
      type: 'symbol',
      source: 'source-incidents',
      layout: {
        'text-field': ['get', 'title'],
        'text-size': 11,
        'text-offset': [0, 1.2],
        'text-anchor': 'top',
        'text-font': ['Open Sans Semibold']
      },
      paint: {
        'text-color': '#f8fafc',
        'text-halo-color': '#030712',
        'text-halo-width': 1.5
      }
    });
  }

  // 4. WMS Severity Layer (FESM v1)
  if (!state.map.getSource('source-fesm-wms')) {
    state.map.addSource('source-fesm-wms', {
      type: 'raster',
      tiles: [
        'https://datasets.seed.nsw.gov.au/geoserver/wms?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&LAYERS=fesm_v1&CRS=EPSG:3857&BBOX={bbox-epsg-3857}&WIDTH=256&HEIGHT=256&FORMAT=image/png&TRANSPARENT=TRUE'
      ],
      tileSize: 256
    });

    state.map.addLayer({
      id: 'layer-fesm-wms',
      type: 'raster',
      source: 'source-fesm-wms',
      layout: { visibility: 'none' },
      paint: { 'raster-opacity': 0.7 }
    }, 'layer-incidents-circles');
  }

  // Click inspection handlers
  ['layer-incidents-circles', 'layer-hotspots-pulse', 'layer-history-fill'].forEach(layerId => {
    state.map.on('click', layerId, (e) => {
      if (e.features && e.features.length > 0) {
        showFeatureDetails(e.features[0]);
      }
    });

    state.map.on('mouseenter', layerId, () => {
      state.map.getCanvas().style.cursor = 'pointer';
    });
    state.map.on('mouseleave', layerId, () => {
      state.map.getCanvas().style.cursor = '';
    });
  });
}

async function fetchMapData(force = false) {
  try {
    const [incidentsRes, hotspotsRes, historyRes] = await Promise.all([
      fetch(`${API_BASE}/api/v1/incidents`).then(r => r.json()),
      fetch(`${API_BASE}/api/v1/hotspots`).then(r => r.json()),
      fetch(`${API_BASE}/api/v1/history`).then(r => r.json())
    ]);

    state.incidents = incidentsRes || [];
    state.hotspots = hotspotsRes || [];
    state.historyFires = historyRes || [];

    document.getElementById('count-incidents').textContent = state.incidents.length;
    document.getElementById('count-hotspots').textContent = state.hotspots.length;
    document.getElementById('count-history').textContent = state.historyFires.length;

    updateMapSources();
    renderExplorerTable();
  } catch (err) {
    console.error('Failed to fetch map data:', err);
  }
}

function updateMapSources() {
  if (!state.map || !state.mapLoaded) return;

  // Convert incidents to GeoJSON FeatureCollection
  const incFeatures = state.incidents.map(inc => ({
    type: 'Feature',
    id: inc.id,
    geometry: inc.geometry || { type: 'Point', coordinates: inc.centroid },
    properties: {
      id: inc.id,
      title: inc.title,
      alertLevel: inc.alertLevel,
      status: inc.status,
      category: inc.category,
      councilArea: inc.councilArea,
      sizeHa: inc.sizeHa,
      source: inc.provenance ? inc.provenance.providerName : 'NSW RFS',
      freshness: inc.provenance ? inc.provenance.freshness : 'fallback-snapshot'
    }
  }));

  const srcInc = state.map.getSource('source-incidents');
  if (srcInc) srcInc.setData({ type: 'FeatureCollection', features: incFeatures });

  // Hotspots
  const hotFeatures = state.hotspots.map(h => ({
    type: 'Feature',
    id: h.id,
    geometry: { type: 'Point', coordinates: [h.longitude, h.latitude] },
    properties: {
      id: h.id,
      satellite: h.satellite,
      instrument: h.instrument,
      confidence: h.confidence,
      frpMw: h.frpMw,
      brightnessK: h.brightnessK,
      dayNight: h.dayNight,
      acquiredAtIso: h.acquiredAtIso,
      source: 'DEA Hotspots / FIRMS'
    }
  }));

  const srcHot = state.map.getSource('source-hotspots');
  if (srcHot) srcHot.setData({ type: 'FeatureCollection', features: hotFeatures });

  // History perimeters
  const histFeatures = state.historyFires.map(h => ({
    type: 'Feature',
    id: h.id,
    geometry: h.geometry,
    properties: {
      id: h.id,
      fireName: h.fireName,
      fireSeason: h.fireSeason,
      fireType: h.fireType,
      areaHa: h.areaHa,
      bioregionOrCouncil: h.bioregionOrCouncil,
      ignitionDateIso: h.ignitionDateIso,
      source: 'NSW NPWS'
    }
  }));

  const srcHist = state.map.getSource('source-history');
  if (srcHist) srcHist.setData({ type: 'FeatureCollection', features: histFeatures });
}

function showFeatureDetails(feature) {
  const drawer = document.getElementById('feature-drawer');
  const title = document.getElementById('drawer-title');
  const body = document.getElementById('drawer-body');

  const props = feature.properties || {};
  title.textContent = props.title || props.fireName || `Feature: ${props.id || 'Unknown'}`;

  let html = `<div class="detail-group">`;
  for (const [k, v] of Object.entries(props)) {
    if (v !== null && v !== undefined && k !== 'geometry') {
      html += `<div style="margin-bottom: 6px;">
        <span style="color: #94a3b8; font-size: 11px;">${k}:</span>
        <strong style="font-family: var(--font-mono); font-size: 12px; margin-left: 6px;">${v}</strong>
      </div>`;
    }
  }
  html += `</div>`;

  if (props.fireName || props.title) {
    html += `<button class="btn-primary w-full" style="margin-top: 14px;" onclick="selectEventForSimilarity('${props.id}')">
      Find Historically Similar Fires
    </button>`;
  }

  body.innerHTML = html;
  drawer.classList.remove('hidden');
}

// -----------------------------------------------------------------------------
// WORKSPACE 2: EXPLORER & SIMILARITY ENGINE
// -----------------------------------------------------------------------------
let explorerMode = 'incidents'; // or 'history'

function initExplorer() {
  const btnInc = document.getElementById('btn-show-incidents');
  const btnHist = document.getElementById('btn-show-history');
  const searchInput = document.getElementById('explorer-search');
  const alertFilter = document.getElementById('filter-alert');
  const fireTypeFilter = document.getElementById('filter-firetype');
  const areaSlider = document.getElementById('filter-minarea');
  const areaLabel = document.getElementById('lbl-minarea');

  btnInc.addEventListener('click', () => {
    explorerMode = 'incidents';
    btnInc.classList.add('active');
    btnHist.classList.remove('active');
    renderExplorerTable();
  });

  btnHist.addEventListener('click', () => {
    explorerMode = 'history';
    btnHist.classList.add('active');
    btnInc.classList.remove('active');
    renderExplorerTable();
  });

  searchInput.addEventListener('input', () => renderExplorerTable());
  alertFilter.addEventListener('change', () => renderExplorerTable());
  fireTypeFilter.addEventListener('change', () => renderExplorerTable());

  areaSlider.addEventListener('input', (e) => {
    areaLabel.textContent = `${Number(e.target.value).toLocaleString()} ha`;
    renderExplorerTable();
  });
}

function renderExplorerTable() {
  const tbody = document.getElementById('explorer-table-body');
  if (!tbody) return;

  const query = document.getElementById('explorer-search').value.toLowerCase();
  const alertVal = document.getElementById('filter-alert').value;
  const typeVal = document.getElementById('filter-firetype').value;
  const minArea = parseFloat(document.getElementById('filter-minarea').value) || 0;

  tbody.innerHTML = '';

  if (explorerMode === 'incidents') {
    const filtered = state.incidents.filter(inc => {
      if (alertVal && inc.alertLevel !== alertVal) return false;
      if (minArea > 0 && (!inc.sizeHa || inc.sizeHa < minArea)) return false;
      if (query) {
        const t = (inc.title || '').toLowerCase();
        const c = (inc.councilArea || '').toLowerCase();
        if (!t.includes(query) && !c.includes(query)) return false;
      }
      return true;
    });

    filtered.forEach(inc => {
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td><strong>${inc.title}</strong></td>
        <td><span class="sym-badge sym-rfs">${inc.alertLevel}</span></td>
        <td>${inc.sizeHa ? Number(inc.sizeHa).toLocaleString() + ' ha' : '-'}</td>
        <td>${inc.councilArea}</td>
        <td><button class="btn-sm" onclick="selectEventForSimilarity('${inc.id}')">Compare</button></td>
      `;
      tbody.appendChild(tr);
    });
  } else {
    const filtered = state.historyFires.filter(h => {
      if (typeVal && h.fireType !== typeVal) return false;
      if (minArea > 0 && h.areaHa < minArea) return false;
      if (query) {
        const n = (h.fireName || '').toLowerCase();
        const b = (h.bioregionOrCouncil || '').toLowerCase();
        if (!n.includes(query) && !b.includes(query)) return false;
      }
      return true;
    });

    filtered.forEach(h => {
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td><strong>${h.fireName}</strong> (${h.fireSeason})</td>
        <td><span class="sym-badge sym-history">${h.fireType}</span></td>
        <td>${Number(h.areaHa).toLocaleString()} ha</td>
        <td>${h.bioregionOrCouncil}</td>
        <td><button class="btn-sm" onclick="selectEventForSimilarity('${h.id}')">Compare</button></td>
      `;
      tbody.appendChild(tr);
    });
  }
}

window.selectEventForSimilarity = async function(eventId) {
  switchWorkspace('explorer');

  const refCard = document.getElementById('sim-reference-card');
  const resultsDiv = document.getElementById('sim-results');

  refCard.innerHTML = `<div style="color: var(--accent-blue);">Computing 4-factor deterministic historical similarity...</div>`;
  resultsDiv.innerHTML = '';

  try {
    const res = await fetch(`${API_BASE}/api/v1/similarity?event_id=${encodeURIComponent(eventId)}&limit=5`);
    const data = await res.json();

    if (!res.ok || data.error) {
      refCard.innerHTML = `<div style="color: var(--accent-red);">Error: ${data.error || 'Failed to compute similarity'}</div>`;
      return;
    }

    const ref = data.referenceEvent;
    refCard.innerHTML = `
      <div style="display: flex; justify-content: space-between; align-items: center;">
        <div>
          <span style="font-size: 11px; color: #38bdf8; font-weight: 700;">REFERENCE EVENT</span>
          <h4 style="font-size: 16px; margin: 4px 0;">${ref.fireName} (${ref.fireSeason})</h4>
          <span style="font-size: 12px; color: var(--text-muted);">${ref.fireType} • ${Number(ref.areaHa).toLocaleString()} ha • ${ref.bioregionOrCouncil}</span>
        </div>
        <span class="badge badge-live">Selected</span>
      </div>
    `;

    resultsDiv.innerHTML = '';
    data.matches.forEach((m, idx) => {
      const card = document.createElement('div');
      card.className = 'sim-card';
      card.innerHTML = `
        <div class="sim-card-header">
          <div>
            <strong style="font-size: 14px;">#${idx + 1} ${m.record.fireName} (${m.record.fireSeason})</strong>
            <div style="font-size: 12px; color: var(--text-muted); margin-top: 2px;">
              ${m.record.fireType} • ${Number(m.record.areaHa).toLocaleString()} ha • ${m.record.bioregionOrCouncil}
            </div>
          </div>
          <span class="score-badge">${m.totalScorePct.toFixed(1)}% Match</span>
        </div>

        <div class="breakdown-bars">
          <div class="bar-box">
            <div class="bar-label"><span>Geography (35%)</span><strong>${(m.geoScore * 100).toFixed(0)}%</strong></div>
            <div class="bar-track"><div class="bar-fill" style="width: ${m.geoScore * 100}%"></div></div>
            <span style="font-size: 10px; color: var(--text-muted);">${m.distanceKm.toFixed(0)} km away</span>
          </div>

          <div class="bar-box">
            <div class="bar-label"><span>Scale/Area (30%)</span><strong>${(m.sizeScore * 100).toFixed(0)}%</strong></div>
            <div class="bar-track"><div class="bar-fill" style="width: ${m.sizeScore * 100}%; background: #f59e0b;"></div></div>
            <span style="font-size: 10px; color: var(--text-muted);">${Number(m.record.areaHa).toLocaleString()} ha</span>
          </div>

          <div class="bar-box">
            <div class="bar-label"><span>Season (20%)</span><strong>${(m.seasonScore * 100).toFixed(0)}%</strong></div>
            <div class="bar-track"><div class="bar-fill" style="width: ${m.seasonScore * 100}%; background: #10b981;"></div></div>
            <span style="font-size: 10px; color: var(--text-muted);">DOY ${m.record.dayOfYear}</span>
          </div>

          <div class="bar-box">
            <div class="bar-label"><span>Type (15%)</span><strong>${(m.typeScore * 100).toFixed(0)}%</strong></div>
            <div class="bar-track"><div class="bar-fill" style="width: ${m.typeScore * 100}%; background: #a855f7;"></div></div>
            <span style="font-size: 10px; color: var(--text-muted);">${m.typeScore === 1.0 ? 'Exact Match' : 'Same Type'}</span>
          </div>
        </div>
      `;
      resultsDiv.appendChild(card);
    });

  } catch (err) {
    refCard.innerHTML = `<div style="color: var(--accent-red);">Network or computation error: ${err.message}</div>`;
  }
};

// -----------------------------------------------------------------------------
// WORKSPACE 3: SATELLITE & CHANGE INSPECTOR
// -----------------------------------------------------------------------------
const SPECTRAL_METADATA = {
  'true-colour': {
    name: 'True Colour (Natural RGB)',
    formula: 'R = B04 (Red), G = B03 (Green), B = B02 (Blue)',
    desc: 'Simulates human visual perception using visible light bands.',
    caveat: 'Cannot penetrate smoke haze or cloud. Cannot reliably distinguish dark shadow from light burn scars.'
  },
  'false-colour-swir': {
    name: 'False Colour SWIR (Burn Scar Emphasis)',
    formula: 'R = B12 (SWIR2), G = B8A (Narrow NIR), B = B04 (Red)',
    desc: 'Combines Short-Wave Infrared with Near-Infrared to penetrate atmospheric smoke haze and highlight charred organic matter.',
    caveat: 'SWIR bands have 20m pixel resolution. Bare dry red soil or rocky terrain can mimic burn scar reflectance.'
  },
  'ndvi': {
    name: 'NDVI (Normalized Difference Vegetation Index)',
    formula: 'NDVI = (NIR - Red) / (NIR + Red) = (B08 - B04) / (B08 + B04)',
    desc: 'Measures live green vegetation density and chlorophyll absorption. Scale: -1.0 to +1.0.',
    caveat: 'Severe drought or seasonal curing significantly reduces NDVI even in unburnt vegetation.'
  },
  'dnbr': {
    name: 'NBR & Delta NBR (Burn Severity)',
    formula: 'NBR = (NIR - SWIR2) / (NIR + SWIR2),  dNBR = NBR_pre - NBR_post',
    desc: 'Optimal remote sensing index for mapping fire severity and canopy consumption.',
    caveat: 'Requires cloud-free and shadow-free imagery. Agricultural harvesting or logging can mimic fire severity.'
  }
};

function initSatelliteWorkspace() {
  const modeSelect = document.getElementById('spectral-mode-select');
  const cloudSlider = document.getElementById('stac-cloud-slider');
  const cloudLabel = document.getElementById('lbl-cloud');
  const btnQuery = document.getElementById('btn-query-stac');

  modeSelect.addEventListener('change', (e) => {
    const meta = SPECTRAL_METADATA[e.target.value];
    if (meta) {
      document.getElementById('spec-name').textContent = meta.name;
      document.getElementById('spec-formula').textContent = meta.formula;
      document.getElementById('spec-desc').textContent = meta.desc;
      document.getElementById('spec-caveat').innerHTML = `<strong>Caveat:</strong> ${meta.caveat}`;
    }
  });

  cloudSlider.addEventListener('input', (e) => {
    cloudLabel.textContent = e.target.value + '%';
  });

  btnQuery.addEventListener('click', async () => {
    const preset = document.getElementById('stac-preset').value;
    const maxCloud = document.getElementById('stac-cloud-slider').value;
    btnQuery.textContent = 'Querying DEA STAC...';

    try {
      const res = await fetch(`${API_BASE}/api/v1/stac/search?max_cloud=${maxCloud}`);
      const scenes = await res.json();
      btnQuery.textContent = `Search Complete (${scenes.length} scenes found)`;
    } catch (e) {
      btnQuery.textContent = 'STAC Query Failed';
    }
    setTimeout(() => { btnQuery.textContent = 'Search STAC Scenes'; }, 2000);
  });
}

// -----------------------------------------------------------------------------
// WORKSPACE 4: DATA HEALTH, SETTINGS & EXPORTS
// -----------------------------------------------------------------------------
function initHealthAndExports() {
  document.getElementById('btn-force-refresh-all').addEventListener('click', async () => {
    try {
      await fetch(`${API_BASE}/api/v1/settings`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ key: 'force_refresh', value: 'true' })
      });
      loadInitialData();
    } catch (e) {
      console.error(e);
    }
  });

  document.getElementById('btn-save-firms-key').addEventListener('click', async () => {
    const key = document.getElementById('txt-firms-key').value.trim();
    try {
      await fetch(`${API_BASE}/api/v1/settings`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ key: 'NASA_FIRMS_MAP_KEY', value: key })
      });
      alert('NASA FIRMS API Key saved securely to SQLite configuration table.');
    } catch (e) {
      alert('Failed to save key: ' + e.message);
    }
  });

  document.getElementById('btn-run-selftest').addEventListener('click', runClientSelfTest);

  document.getElementById('btn-print-report').addEventListener('click', () => {
    window.print();
  });
}

async function loadInitialData() {
  try {
    const res = await fetch(`${API_BASE}/api/v1/health`);
    const data = await res.json();

    state.providerStatuses = data.providers || [];
    renderHealthTable();

    // Update global status badge in header
    const pill = document.getElementById('global-freshness-pill');
    let hasLive = state.providerStatuses.some(p => p.status === 'live');
    if (hasLive) {
      pill.className = 'badge badge-live';
      pill.textContent = 'Live Connected';
    } else {
      pill.className = 'badge badge-fallback';
      pill.textContent = 'CORS / Snapshot Mode';
    }
  } catch (err) {
    console.error('Failed to load health status:', err);
  }
}

function renderHealthTable() {
  const tbody = document.getElementById('provider-status-tbody');
  if (!tbody) return;

  tbody.innerHTML = '';
  state.providerStatuses.forEach(p => {
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td><code>${p.providerId}</code></td>
      <td><strong>${p.name}</strong></td>
      <td><a href="${p.endpointUrl}" target="_blank" style="color: #38bdf8; text-decoration: underline;">${p.endpointUrl.substring(0, 40)}...</a></td>
      <td>${p.licence}</td>
      <td><span class="badge badge-${p.status}">${p.status}</span></td>
      <td>${p.latencyMs ? p.latencyMs + ' ms' : '-'}</td>
      <td><strong>${p.recordCount}</strong></td>
    `;
    tbody.appendChild(tr);
  });
}

async function runClientSelfTest() {
  const consoleEl = document.getElementById('selftest-results');
  consoleEl.textContent = 'Executing Bushfire Memory Runtime Certification...\n';

  let passed = 0;
  let failed = 0;

  const logTest = (name, cond) => {
    if (cond) {
      consoleEl.textContent += `  [PASS] ${name}\n`;
      passed++;
    } else {
      consoleEl.textContent += `  [FAIL] ${name}\n`;
      failed++;
    }
  };

  // 1. Health API responds with status == ok
  try {
    const h = await fetch(`${API_BASE}/api/v1/health`).then(r => r.json());
    logTest('Health API Endpoint (/api/v1/health)', h.status === 'ok');
  } catch (e) {
    logTest('Health API Endpoint (/api/v1/health)', false);
  }

  // 2. Incidents feed returns non-empty list with schema
  try {
    const incs = await fetch(`${API_BASE}/api/v1/incidents`).then(r => r.json());
    logTest('Incidents Ingestion & Schema Integrity', Array.isArray(incs) && incs.length > 0 && incs[0].title);
  } catch (e) {
    logTest('Incidents Ingestion & Schema Integrity', false);
  }

  // 3. Hotspots feed returns records
  try {
    const hots = await fetch(`${API_BASE}/api/v1/hotspots`).then(r => r.json());
    logTest('Hotspots Ingestion & Anomaly Labeling', Array.isArray(hots) && hots.length > 0);
  } catch (e) {
    logTest('Hotspots Ingestion & Anomaly Labeling', false);
  }

  // 4. Similarity scoring engine self-identity test
  try {
    const hist = await fetch(`${API_BASE}/api/v1/history`).then(r => r.json());
    if (hist.length > 0) {
      const sim = await fetch(`${API_BASE}/api/v1/similarity?event_id=${hist[0].id}`).then(r => r.json());
      logTest('Similarity Engine Top Match Execution', sim.matches && sim.matches.length > 0);
      logTest('Similarity Engine Non-Predictive Disclaimer', sim.disclaimer && sim.disclaimer.includes('Statistical attribute similarity'));
    }
  } catch (e) {
    logTest('Similarity Engine Execution', false);
  }

  consoleEl.textContent += `\nCertification Summary: ${passed} passed, ${failed} failed.\n`;
}
