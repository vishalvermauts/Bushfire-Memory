#pragma once

#include "bushfire/Types.hpp"
#include <string>

namespace bushfire {

class IDataProviderAdapter {
public:
    virtual ~IDataProviderAdapter() = default;

    virtual std::string getProviderId() const = 0;
    virtual std::string getProviderName() const = 0;
    virtual DataSourceStatus getStatus() = 0;
    virtual void refresh(bool force = false) = 0;
};

} // namespace bushfire
