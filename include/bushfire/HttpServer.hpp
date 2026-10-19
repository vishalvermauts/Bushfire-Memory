#pragma once

#include "bushfire/providers/RfsIncidentAdapter.hpp"
#include "bushfire/providers/HotspotAdapter.hpp"
#include "bushfire/providers/HistoricalFireAdapter.hpp"
#include "bushfire/providers/StacImageryAdapter.hpp"
#include "bushfire/CacheStore.hpp"
#include "httplib.h"
#include <string>
#include <memory>

namespace bushfire {

class HttpServer {
public:
    HttpServer(
        std::shared_ptr<RfsIncidentAdapter> rfs,
        std::shared_ptr<HotspotAdapter> hotspots,
        std::shared_ptr<HistoricalFireAdapter> history,
        std::shared_ptr<StacImageryAdapter> stac,
        std::shared_ptr<CacheStore> cache,
        const std::string& host = "127.0.0.1",
        int port = 8080,
        const std::string& webDir = "web"
    );

    ~HttpServer();

    void setupRoutes();
    bool start();
    void stop();

    std::string getHost() const { return host_; }
    int getPort() const { return port_; }

private:
    std::shared_ptr<RfsIncidentAdapter> rfs_;
    std::shared_ptr<HotspotAdapter> hotspots_;
    std::shared_ptr<HistoricalFireAdapter> history_;
    std::shared_ptr<StacImageryAdapter> stac_;
    std::shared_ptr<CacheStore> cache_;

    std::string host_;
    int port_;
    std::string webDir_;
    std::unique_ptr<httplib::Server> svr_;
};

} // namespace bushfire
