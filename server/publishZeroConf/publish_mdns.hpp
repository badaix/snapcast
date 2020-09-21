#ifndef PUBLISH_MDNS_H
#define PUBLISH_MDNS_H

#include <boost/asio.hpp>
#include <string>
#include <vector>


struct mDNSService
{
    mDNSService(const std::string& name, size_t port) : name_(name), port_(port)
    {
    }

    std::string name_;
    size_t port_;
};


class PublishmDNS
{
public:
    PublishmDNS(const std::string& serviceName, boost::asio::io_context& ioc) : serviceName_(serviceName), ioc_(ioc)
    {
    }

    virtual ~PublishmDNS() = default;

    virtual void publish(const std::vector<mDNSService>& services) = 0;

protected:
    std::string serviceName_;
    boost::asio::io_context& ioc_;
};

#if defined(HAS_AVAHI)
#include "publish_avahi.hpp"
using PublishZeroConf = PublishAvahi;
#elif defined(HAS_BONJOUR)
#include "publish_bonjour.hpp"
using PublishZeroConf = PublishBonjour;
#endif

#endif
