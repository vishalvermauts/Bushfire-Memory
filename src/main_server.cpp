#include "bushfire/HttpServer.hpp"
#include "bushfire/HttpClient.hpp"
#include "bushfire/CacheStore.hpp"
#include <iostream>
#include <csignal>
#include <memory>

static std::shared_ptr<bushfire::HttpServer> g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\n[Bushfire Memory] Caught signal " << signum << ", shutting down..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string webDir = "web";
    std::string dbPath = "bushfire_cache.sqlite3";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--web" && i + 1 < argc) webDir = argv[++i];
        else if (arg == "--db" && i + 1 < argc) dbPath = argv[++i];
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Bushfire Memory Geospatial Server (v3.0 C++20)\n"
                      << "Usage: bushfire_memory_server [options]\n"
                      << "Options:\n"
                      << "  --host <ip>        Host to bind (default: 127.0.0.1)\n"
                      << "  --port <port>      Port to listen on (default: 8080)\n"
                      << "  --web <dir>        Static web directory (default: web)\n"
                      << "  --db <path>        SQLite cache path (default: bushfire_cache.sqlite3)\n"
                      << "  --help, -h         Show this help message\n";
            return 0;
        }
    }

    std::cout << "========================================================\n"
              << "       BUSHFIRE MEMORY GEOSPATIAL INTELLIGENCE PLATFORM\n"
              << "       C++20 Native Server - Australian Bushfire Analysis\n"
              << "========================================================\n";

    bushfire::HttpClient::globalInit();

    auto http = std::make_shared<bushfire::HttpClient>();
    auto cache = std::make_shared<bushfire::CacheStore>(dbPath);

    auto rfs = std::make_shared<bushfire::RfsIncidentAdapter>(http, cache);
    auto hotspots = std::make_shared<bushfire::HotspotAdapter>(http, cache);
    auto history = std::make_shared<bushfire::HistoricalFireAdapter>(http, cache);
    auto stac = std::make_shared<bushfire::StacImageryAdapter>(http, cache);

    // Initial background seed / refresh
    std::cout << "[Initialization] Ingesting initial datasets..." << std::endl;
    rfs->refresh(false);
    hotspots->refresh(false);
    history->refresh(false);
    stac->refresh(false);

    g_server = std::make_shared<bushfire::HttpServer>(rfs, hotspots, history, stac, cache, host, port, webDir);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    bool ok = g_server->start();

    bushfire::HttpClient::globalCleanup();
    return ok ? 0 : 1;
}
