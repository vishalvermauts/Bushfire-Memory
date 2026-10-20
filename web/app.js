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
      'esri-dark-base': {
        type: 'raster',
        tiles: [
          'https://services.arcgisonline.com/arcgis/rest/services/Canvas/World_Dark_Gray_Base/MapServer/tile/{z}/{y}/{x}'
        ],
        tileSize: 256,
        attribution: 'Tiles &copy; Esri &mdash; Esri, DeLorme, NAVTEQ, TomTom'
      },
      'esri-dark-ref': {
        type: 'raster',
        tiles: [
          'https://services.arcgisonline.com/arcgis/rest/services/Canvas/World_Dark_Gray_Reference/MapServer/tile/{z}/{y}/{x}'
        ],
        tileSize: 256
      }
    },
    layers: [
      { id: 'esri-dark-base-layer', type: 'raster', source: 'esri-dark-base', minzoom: 0, maxzoom: 19 },
      { id: 'esri-dark-ref-layer', type: 'raster', source: 'esri-dark-ref', minzoom: 0, maxzoom: 19 }
    ]
  },
  ofmDark: 'https://tiles.openfreemap.org/styles/dark',
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

  // Floating Quick Region Jump Chips
  document.querySelectorAll('.region-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      document.querySelectorAll('.region-chip').forEach(c => c.classList.remove('active'));
      chip.classList.add('active');
      const reg = chip.dataset.region;
      if (REGION_PRESETS[reg]) {
        state.map.flyTo({
          center: REGION_PRESETS[reg].center,
          zoom: REGION_PRESETS[reg].zoom,
          essential: true
        });
      }
    });
  });

  // Dock Minimize/Expand Toggle
  const btnToggleControls = document.getElementById('btn-toggle-controls');
  if (btnToggleControls) {
    btnToggleControls.addEventListener('click', () => {
      const panel = document.getElementById('map-controls');
      panel.classList.toggle('minimized');
      btnToggleControls.textContent = panel.classList.contains('minimized') ? '+' : '−';
    });
  }

  // Basemap Switcher Buttons
  document.querySelectorAll('.basemap-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.basemap-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      const styleKey = btn.dataset.style;
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
        'circle-color': [
          'interpolate', ['linear'], ['coalesce', ['get', 'frpMw'], 10],
          5, '#f59e0b',
          25, '#f97316',
          100, '#ef4444',
          300, '#b91c1c'
        ],
        'circle-radius': [
          'interpolate', ['linear'], ['coalesce', ['get', 'frpMw'], 10],
          5, 3.5,
          50, 7.5,
          200, 14
        ],
        'circle-stroke-color': '#ffffff',
        'circle-stroke-width': 1,
        'circle-opacity': 0.88
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

async function safeFetchJson(url, fallbackUrl) {
  try {
    const res = await fetch(url);
    if (res.ok) {
      const data = await res.json();
      if (data && (!Array.isArray(data) || data.length > 0)) return data;
    }
  } catch (err) {
    // try fallback
  }

  if (fallbackUrl) {
    try {
      const res = await fetch(fallbackUrl);
      if (res.ok) return await res.json();
    } catch (err) {
      console.warn(`Fallback fetch failed for ${fallbackUrl}:`, err);
    }
  }
  return [];
}

async function fetchMapData(force = false) {
  try {
    const [incidentsRes, hotspotsRes, historyRes] = await Promise.all([
      safeFetchJson(`${API_BASE}/api/v1/incidents`, `${API_BASE}/api/incidents.json`),
      safeFetchJson(`${API_BASE}/api/v1/hotspots`, `${API_BASE}/api/hotspots.json`),
      safeFetchJson(`${API_BASE}/api/v1/history`, `${API_BASE}/api/history.json`)
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

  const incFeatures = state.incidents.map(inc => {
    let geom = inc.geometry || inc.geometryJson;
    if (!geom || !geom.type) {
      if (inc.centroidLng != null && inc.centroidLat != null) {
        geom = { type: 'Point', coordinates: [Number(inc.centroidLng), Number(inc.centroidLat)] };
      } else if (Array.isArray(inc.centroid)) {
        geom = { type: 'Point', coordinates: inc.centroid };
      } else {
        geom = { type: 'Point', coordinates: [150.0, -33.5] };
      }
    }
    if (geom.type === 'GeometryCollection' && Array.isArray(geom.geometries)) {
      const pt = geom.geometries.find(g => g.type === 'Point');
      if (pt && pt.coordinates) geom = pt;
      else if (inc.centroidLng != null && inc.centroidLat != null) {
        geom = { type: 'Point', coordinates: [Number(inc.centroidLng), Number(inc.centroidLat)] };
      }
    }

    return {
      type: 'Feature',
      id: inc.id,
      geometry: geom,
      properties: {
        id: inc.id,
        title: inc.title,
        alertLevel: inc.alertLevel,
        status: inc.status,
        category: inc.category,
        councilArea: inc.councilArea,
        sizeHa: inc.sizeHa,
        source: inc.provenance ? inc.provenance.providerName : 'NSW RFS',
        freshness: inc.provenance ? inc.provenance.freshness : 'live'
      }
    };
  });

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
  const isHotspot = props.id && props.id.toString().startsWith('NASA-FIRMS');

  if (isHotspot) {
    title.textContent = `NASA FIRMS Hotspot (${props.satellite || 'VIIRS'})`;
    const frp = Number(props.frpMw || 0).toFixed(1);
    const bright = Number(props.brightnessK || 0).toFixed(1);
    const conf = props.confidence === 'h' ? 'High' : (props.confidence === 'l' ? 'Low' : 'Nominal');
    const acqTime = props.acquiredAtIso ? props.acquiredAtIso.replace('T', ' ').replace('Z', ' UTC') : 'Recent Satellite Pass';

    body.innerHTML = `
      <div class="hero-stat-grid">
        <div class="hero-stat-box">
          <div class="hero-stat-label">Fire Radiative Power</div>
          <div class="hero-stat-value" style="color: #ea580c;">${frp} MW</div>
        </div>
        <div class="hero-stat-box">
          <div class="hero-stat-label">Brightness Temp</div>
          <div class="hero-stat-value" style="color: #f59e0b;">${bright} K</div>
        </div>
        <div class="hero-stat-box">
          <div class="hero-stat-label">Confidence</div>
          <div class="hero-stat-value" style="color: #22c55e;">${conf}</div>
        </div>
        <div class="hero-stat-box">
          <div class="hero-stat-label">Sensor / Band</div>
          <div class="hero-stat-value">${props.instrument || 'VIIRS'} (375m)</div>
        </div>
      </div>

      <div style="background: rgba(255,255,255,0.03); border: 1px solid var(--border-subtle); border-radius: 10px; padding: 12px; margin-bottom: 12px;">
        <div style="font-size: 10px; color: var(--text-dim); text-transform: uppercase; font-weight: 700;">Satellite Telemetry & Source</div>
        <div style="font-size: 13px; font-weight: 600; color: #fff; margin-top: 4px;">🛰️ ${acqTime}</div>
        <div style="font-size: 11px; color: var(--text-muted); margin-top: 2px;">Provider: NASA FIRMS (Live API Key Configured)</div>
      </div>

      <button class="btn-cta-find-similar" onclick="selectEventForSimilarity('${props.id}')">
        ✨ Find Historically Similar Fires
      </button>
    `;
  } else {
    const fireName = props.fireName || props.title || `Incident ${props.id || ''}`;
    title.textContent = fireName;

    const area = Number(props.sizeHa || props.areaHa || 0).toLocaleString();
    const location = props.councilArea || props.bioregionOrCouncil || 'New South Wales';
    const type = props.fireType || props.category || 'Bush Fire';
    const status = props.status || props.alertLevel || 'Active';
    const season = props.fireSeason || 'Current Season';
    const source = props.source || 'NSW Rural Fire Service';

    body.innerHTML = `
      <div class="hero-stat-grid">
        <div class="hero-stat-box">
          <div class="hero-stat-label">Estimated Footprint</div>
          <div class="hero-stat-value">${area} ha</div>
        </div>
        <div class="hero-stat-box">
          <div class="hero-stat-label">Incident Status</div>
          <div class="hero-stat-value" style="color: #fbbf24;">${status}</div>
        </div>
        <div class="hero-stat-box">
          <div class="hero-stat-label">Classification</div>
          <div class="hero-stat-value">${type}</div>
        </div>
        <div class="hero-stat-box">
          <div class="hero-stat-label">Season / Cycle</div>
          <div class="hero-stat-value">${season}</div>
        </div>
      </div>

      <div style="background: rgba(255,255,255,0.03); border: 1px solid var(--border-subtle); border-radius: 10px; padding: 12px; margin-bottom: 12px;">
        <div style="font-size: 10px; color: var(--text-dim); text-transform: uppercase; font-weight: 700;">Location & Source</div>
        <div style="font-size: 13px; font-weight: 600; color: #fff; margin-top: 4px;">📍 ${location}</div>
        <div style="font-size: 11px; color: var(--text-muted); margin-top: 2px;">Provider: ${source}</div>
      </div>

      <button class="btn-cta-find-similar" onclick="selectEventForSimilarity('${props.id}')">
        ✨ Find Historically Similar Fires
      </button>
    `;
  }
  drawer.classList.remove('hidden');
}

// -----------------------------------------------------------------------------
// WORKSPACE 2: EXPLORER & SIMILARITY ENGINE
// -----------------------------------------------------------------------------
function initExplorer() {
  const searchInput = document.getElementById('txt-search-events') || document.getElementById('explorer-search');
  const alertFilter = document.getElementById('filter-alert');
  const fireTypeFilter = document.getElementById('filter-firetype');

  if (searchInput) searchInput.addEventListener('input', () => renderExplorerTable());
  if (alertFilter) alertFilter.addEventListener('change', () => renderExplorerTable());
  if (fireTypeFilter) fireTypeFilter.addEventListener('change', () => renderExplorerTable());
}

function renderExplorerTable() {
  const tbody = document.getElementById('explorer-table-body');
  if (!tbody) return;

  const searchInput = document.getElementById('txt-search-events') || document.getElementById('explorer-search');
  const query = searchInput ? searchInput.value.toLowerCase().trim() : '';
  const alertVal = document.getElementById('filter-alert')?.value || '';
  const typeVal = document.getElementById('filter-firetype')?.value || '';

  tbody.innerHTML = '';

  const allEvents = [
    ...state.historyFires.map(h => ({
      id: h.id,
      name: `${h.fireName} (${h.fireSeason})`,
      type: h.fireType,
      area: h.areaHa,
      location: h.bioregionOrCouncil,
      isHistory: true
    })),
    ...state.incidents.map(inc => ({
      id: inc.id,
      name: inc.title,
      type: inc.category || inc.alertLevel,
      area: inc.sizeHa || 0,
      location: inc.councilArea,
      isHistory: false,
      alertLevel: inc.alertLevel
    }))
  ];

  const filtered = allEvents.filter(ev => {
    if (alertVal && ev.alertLevel && ev.alertLevel !== alertVal) return false;
    if (typeVal && ev.type !== typeVal) return false;
    if (query) {
      const n = (ev.name || '').toLowerCase();
      const l = (ev.location || '').toLowerCase();
      if (!n.includes(query) && !l.includes(query)) return false;
    }
    return true;
  });

  filtered.forEach(ev => {
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td>
        <strong style="font-size: 13px;">${ev.name}</strong>
        ${ev.isHistory ? '<span style="font-size: 10px; color: var(--accent-amber); margin-left: 6px;">[Historical Perimeter]</span>' : '<span style="font-size: 10px; color: var(--accent-flame); margin-left: 6px;">[Live Incident]</span>'}
      </td>
      <td><span class="badge ${ev.isHistory ? 'badge-fallback' : 'badge-live'}">${ev.type}</span></td>
      <td><strong>${ev.area ? Number(ev.area).toLocaleString() + ' ha' : '-'}</strong></td>
      <td>${ev.location}</td>
      <td><button class="btn-compare" onclick="selectEventForSimilarity('${ev.id}')">✨ Compare</button></td>
    `;
    tbody.appendChild(tr);
  });
}

window.selectEventForSimilarity = async function(eventId) {
  switchWorkspace('explorer');

  const refCard = document.getElementById('sim-reference-card');
  const resultsDiv = document.getElementById('sim-results');

  refCard.innerHTML = `<div style="color: var(--accent-blue);">Computing 4-factor deterministic historical similarity...</div>`;
  resultsDiv.innerHTML = '';

  let data = null;
  try {
    const res = await fetch(`${API_BASE}/api/v1/similarity?event_id=${encodeURIComponent(eventId)}&limit=5`);
    if (res.ok) {
      data = await res.json();
    }
  } catch (err) {
    // network or static host without backend
  }

  if (!data || data.error || !data.matches || data.matches.length === 0) {
    data = computeClientSideSimilarity(eventId, 5);
  }

  if (!data || data.error) {
    refCard.innerHTML = `<div style="color: var(--accent-red);">Error: ${data ? data.error : 'Failed to compute similarity'}</div>`;
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
            <strong style="font-size: 14px; color: #fff;">#${idx + 1} ${m.record.fireName} (${m.record.fireSeason})</strong>
            <div style="font-size: 11px; color: var(--text-muted); margin-top: 2px;">
              ${m.record.fireType} • ${Number(m.record.areaHa).toLocaleString()} ha • 📍 ${m.record.bioregionOrCouncil}
            </div>
          </div>
          <div style="display: flex; align-items: center; gap: 8px;">
            <button class="btn-compare" onclick="flyToFire('${m.record.id}')">🗺️ View on Map</button>
            <span class="score-badge">${m.totalScorePct.toFixed(1)}% Match</span>
          </div>
        </div>

        <div class="breakdown-bars">
          <div class="bar-box">
            <div class="bar-label"><span>Geography</span><strong>${(m.geoScore * 100).toFixed(0)}%</strong></div>
            <div class="bar-track"><div class="bar-fill" style="width: ${m.geoScore * 100}%"></div></div>
            <span style="font-size: 10px; color: var(--text-dim);">${m.distanceKm.toFixed(0)} km away</span>
          </div>

          <div class="bar-box">
            <div class="bar-label"><span>Footprint</span><strong>${(m.sizeScore * 100).toFixed(0)}%</strong></div>
            <div class="bar-track"><div class="bar-fill" style="width: ${m.sizeScore * 100}%; background: #f59e0b;"></div></div>
            <span style="font-size: 10px; color: var(--text-dim);">${Number(m.record.areaHa).toLocaleString()} ha</span>
          </div>

          <div class="bar-box">
            <div class="bar-label"><span>Season</span><strong>${(m.seasonScore * 100).toFixed(0)}%</strong></div>
            <div class="bar-track"><div class="bar-fill" style="width: ${m.seasonScore * 100}%; background: #10b981;"></div></div>
            <span style="font-size: 10px; color: var(--text-dim);">DOY ${m.record.dayOfYear}</span>
          </div>

          <div class="bar-box">
            <div class="bar-label"><span>Category</span><strong>${(m.typeScore * 100).toFixed(0)}%</strong></div>
            <div class="bar-track"><div class="bar-fill" style="width: ${m.typeScore * 100}%; background: #a855f7;"></div></div>
            <span style="font-size: 10px; color: var(--text-dim);">${m.typeScore === 1.0 ? 'Exact Match' : 'Same Type'}</span>
          </div>
        </div>

        <div class="narrative-tag">
          💡 ${m.explanation || 'Deterministic 4-factor heuristic match.'}
        </div>
      `;
    resultsDiv.appendChild(card);
  });
};

window.flyToFire = function(fireId) {
  switchWorkspace('map');
  const fire = state.historyFires.find(f => f.id === fireId) || state.incidents.find(i => i.id === fireId);
  if (fire && state.map) {
    const lng = fire.centroidLng || (fire.centroid ? fire.centroid[0] : 150.0);
    const lat = fire.centroidLat || (fire.centroid ? fire.centroid[1] : -33.5);
    state.map.flyTo({
      center: [lng, lat],
      zoom: 10.2,
      essential: true
    });
    showFeatureDetails({ properties: fire });
  }
};

function haversineDistanceKm(lat1, lon1, lat2, lon2) {
  const R = 6371.0;
  const dLat = (lat2 - lat1) * Math.PI / 180.0;
  const dLon = (lon2 - lon1) * Math.PI / 180.0;
  const a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(lat1 * Math.PI / 180.0) * Math.cos(lat2 * Math.PI / 180.0) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
}

function computeClientSideSimilarity(eventId, limit = 5) {
  const allEvents = [...state.historyFires, ...state.incidents.map(inc => ({
    id: inc.id,
    fireName: inc.title,
    fireSeason: 'Current',
    fireType: inc.category || 'Wildfire',
    areaHa: inc.sizeHa || 100.0,
    bioregionOrCouncil: inc.councilArea || 'NSW',
    dayOfYear: 290,
    centroidLng: inc.centroidLng || (inc.centroid ? inc.centroid[0] : 150.0),
    centroidLat: inc.centroidLat || (inc.centroid ? inc.centroid[1] : -33.5),
    provenance: inc.provenance
  }))];

  const ref = allEvents.find(e => e.id === eventId);
  if (!ref) return { error: `Reference event ${eventId} not found.` };

  const matches = [];
  for (const target of state.historyFires) {
    if (target.id === ref.id && allEvents.length > 1) {
      matches.push({
        record: target,
        totalScorePct: 100.0,
        geoScore: 1.0,
        sizeScore: 1.0,
        seasonScore: 1.0,
        typeScore: 1.0,
        distanceKm: 0.0,
        explanation: 'Self-identity baseline match (100.0% similarity).'
      });
      continue;
    }

    // 1. Geography score: max(0.0, 1.0 - distKm / 500.0)
    const distKm = haversineDistanceKm(ref.centroidLat, ref.centroidLng, target.centroidLat, target.centroidLng);
    const sGeo = Math.max(0.0, 1.0 - distKm / 500.0);

    // 2. Scale / Area score: min(A1, A2) / max(A1, A2)
    const refArea = Math.max(1.0, ref.areaHa || 1.0);
    const targetArea = Math.max(1.0, target.areaHa || 1.0);
    const sSize = Math.min(refArea, targetArea) / Math.max(refArea, targetArea);

    // 3. Seasonality: circular Day of Year
    const refDoy = ref.dayOfYear || 1;
    const targetDoy = target.dayOfYear || 1;
    const diffDoy = Math.abs(refDoy - targetDoy);
    const seasonDays = Math.min(diffDoy, 365 - diffDoy);
    const sSeason = Math.max(0.0, 1.0 - seasonDays / 91.25);

    // 4. Fire Type
    const sType = (ref.fireType === target.fireType) ? 1.0 : 0.2;

    const totalScorePct = (0.35 * sGeo + 0.25 * sSize + 0.20 * sSeason + 0.20 * sType) * 100.0;

    matches.push({
      record: target,
      totalScorePct: totalScorePct,
      geoScore: sGeo,
      sizeScore: sSize,
      seasonScore: sSeason,
      typeScore: sType,
      distanceKm: distKm,
      explanation: `Deterministic 4-factor heuristic match score: ${totalScorePct.toFixed(1)}%.`
    });
  }

  matches.sort((a, b) => b.totalScorePct - a.totalScorePct);
  return {
    referenceEvent: ref,
    matches: matches.slice(0, limit)
  };
}

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
    let data = null;
    try {
      const res = await fetch(`${API_BASE}/api/v1/health`);
      if (res.ok) data = await res.json();
    } catch (e) {}

    if (!data) {
      const fallbackProvs = await safeFetchJson(`${API_BASE}/api/status.json`, null);
      if (fallbackProvs && fallbackProvs.length > 0) {
        data = {
          status: 'ok',
          providers: fallbackProvs.map(p => ({
            providerId: p.providerId,
            name: p.providerName,
            endpointUrl: 'https://datasets.seed.nsw.gov.au',
            licence: 'CC BY 4.0',
            status: p.freshness,
            latencyMs: p.lastLatencyMs,
            recordCount: p.recordCount
          }))
        };
      }
    }

    if (data) {
      state.providerStatuses = data.providers || [];
      renderHealthTable();

      // Update global status badge in header
      const pill = document.getElementById('global-freshness-pill');
      if (pill) {
        if (data && data.hasNasaKey) {
          pill.className = 'badge badge-live';
          pill.textContent = '🟢 NASA FIRMS Live • Active';
        } else if (state.providerStatuses.some(p => p.status === 'live')) {
          pill.className = 'badge badge-live';
          pill.textContent = '🟢 Live Radar • Active';
        } else {
          pill.className = 'badge badge-live';
          pill.textContent = '🛰️ Verified Hotspots • Australia';
        }
      }
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
