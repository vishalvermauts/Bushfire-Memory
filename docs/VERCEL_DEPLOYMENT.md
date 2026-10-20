# Deploying Bushfire Memory to Vercel

Bushfire Memory can be hosted on **Vercel** either as a **standalone static edge console** or as a **hybrid deployment** paired with a containerized C++20 engine.

---

## Architecture Compatibility Summary

| Component | Vercel Native Support | Deployment Strategy |
| :--- | :--- | :--- |
| **MapLibre Frontend (`web/`)** | **100% Native** | Deploys directly to Vercel's Edge Global CDN with zero Node.js build dependencies. |
| **Verified Data Snapshots** | **100% Native** | Pre-rendered JSON endpoints (`/api/v1/incidents`, `/api/v1/history`, `/api/v1/hotspots`) served at static CDN speeds. |
| **Deterministic 4-Factor Matching** | **100% Native** | Executed client-side in the browser via JavaScript using the identical mathematical heuristic ($S_{\text{geo}}, S_{\text{size}}, S_{\text{season}}, S_{\text{type}}$) and Haversine distance. |
| **C++20 REST Engine (`cpp-httplib`, SQLite3)** | **Requires Container Host** | Long-running C++ Linux daemons cannot run directly inside Vercel serverless lambdas. Run on Fly.io, Railway, Render, or AWS, and reverse-proxy via `vercel.json`. |

---

## Method 1: Instant Standalone Deployment (100% Vercel / Free Tier)

Deploy the full client-side intelligence platform directly from your GitHub repository with zero server costs:

### Step 1: Import Project in Vercel
1. Log into your [Vercel Dashboard](https://vercel.com/dashboard).
2. Click **"Add New..."** → **"Project"**.
3. Select your GitHub repository: `https://github.com/vishalvermauts/Bushfire-Memory.git`.

### Step 2: Configure Project Settings
- **Framework Preset**: `Other`
- **Root Directory**: `./` (leave default, `vercel.json` specifies `"outputDirectory": "web"`)
- **Build Command**: Leave empty (no build step needed!)
- **Output Directory**: `web` (automatically detected from `vercel.json`)

### Step 3: Click "Deploy"
Within 15–30 seconds, Vercel will provide a live production URL:
`https://bushfire-memory.vercel.app`

#### What Works in Standalone Vercel Mode:
- **Interactive 4-Workspace Console**: MapLibre GL JS loads vector tiles, terrain, and satellite imagery.
- **Historical Fire Perimeters**: Full polygon boundaries for Gospers Mountain (512k ha), Currowan (314k ha), Dunns Road (333k ha), Green Wattle Creek, Warrumbungle, Canberra, and Prescribed Burns.
- **Satellite Hotspot & Incident Inspection**: Interactive popups and attributes.
- **Client-Side 4-Factor Matcher**: Selecting any fire instantly computes the top-5 historical analogues deterministically with explainable percentage breakdowns.
- **Satellite Indices & Caveats**: True Colour RGB, SWIR False Colour, NDVI, and dNBR formulas and remote sensing disclosures.
- **Client-Side Research Exports**: GeoJSON, CSV, and Markdown generation.

---

## Method 2: Hybrid Production Deployment (Vercel Frontend + C++20 Container)

For continuous background scraping of live NSW RFS feeds, live Geoscience Australia WFS queries, and multi-threaded SQLite caching, run the backend on a container platform and let Vercel handle edge routing.

### Step 1: Deploy C++20 Container to Fly.io (or Railway / Render)
Using the pre-configured [Dockerfile](file:///c:/AI/Bushfire%20Memory/Dockerfile):

```bash
# Example: Deploying to Fly.io
fly launch --image vishalvermauts/bushfire-memory:latest --name bushfire-api
fly deploy
# Your API is now live at: https://bushfire-api.fly.dev
```

### Step 2: Route Vercel to your Container API
Update `vercel.json` rewrites to proxy `/api/*` directly to your container URL:

```json
{
  "$schema": "https://openapi.vercel.sh/vercel.json",
  "cleanUrls": true,
  "outputDirectory": "web",
  "rewrites": [
    {
      "source": "/api/(.*)",
      "destination": "https://bushfire-api.fly.dev/api/$1"
    }
  ]
}
```

Push this commit to `main`, and Vercel will automatically redeploy. Your frontend and backend will now communicate seamlessly with zero CORS configuration required!
