#include "browse_bonjour.hpp"

#include <deque>
#include <iostream>
#include <memory>
#ifdef WINDOWS
#include <WinSock2.h>
#include <Ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#endif

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"

using namespace std;

static constexpr auto LOG_TAG = "Bonjour";

struct DNSServiceRefDeleter
{
    void operator()(DNSServiceRef* ref)
    {
        DNSServiceRefDeallocate(*ref);
        delete ref;
    }
};

using DNSServiceHandle = std::unique_ptr<DNSServiceRef, DNSServiceRefDeleter>;

string BonjourGetError(DNSServiceErrorType error)
{
    switch (error)
    {
        case kDNSServiceErr_NoError:
            return "NoError";

        default:
        case kDNSServiceErr_Unknown:
            return "Unknown";

        case kDNSServiceErr_NoSuchName:
            return "NoSuchName";

        case kDNSServiceErr_NoMemory:
            return "NoMemory";

        case kDNSServiceErr_BadParam:
            return "BadParam";

        case kDNSServiceErr_BadReference:
            return "BadReference";

        case kDNSServiceErr_BadState:
            return "BadState";

        case kDNSServiceErr_BadFlags:
            return "BadFlags";

        case kDNSServiceErr_Unsupported:
            return "Unsupported";

        case kDNSServiceErr_NotInitialized:
            return "NotInitialized";

        case kDNSServiceErr_AlreadyRegistered:
            return "AlreadyRegistered";

        case kDNSServiceErr_NameConflict:
            return "NameConflict";

        case kDNSServiceErr_Invalid:
            return "Invalid";

        case kDNSServiceErr_Firewall:
            return "Firewall";

        case kDNSServiceErr_Incompatible:
            return "Incompatible";

        case kDNSServiceErr_BadInterfaceIndex:
            return "BadInterfaceIndex";

        case kDNSServiceErr_Refused:
            return "Refused";

        case kDNSServiceErr_NoSuchRecord:
            return "NoSuchRecord";

        case kDNSServiceErr_NoAuth:
            return "NoAuth";

        case kDNSServiceErr_NoSuchKey:
            return "NoSuchKey";

        case kDNSServiceErr_NATTraversal:
            return "NATTraversal";

        case kDNSServiceErr_DoubleNAT:
            return "DoubleNAT";

        case kDNSServiceErr_BadTime:
            return "BadTime";

        case kDNSServiceErr_BadSig:
            return "BadSig";

        case kDNSServiceErr_BadKey:
            return "BadKey";

        case kDNSServiceErr_Transient:
            return "Transient";

        case kDNSServiceErr_ServiceNotRunning:
            return "ServiceNotRunning";

        case kDNSServiceErr_NATPortMappingUnsupported:
            return "NATPortMappingUnsupported";

        case kDNSServiceErr_NATPortMappingDisabled:
            return "NATPortMappingDisabled";

        case kDNSServiceErr_NoRouter:
            return "NoRouter";

        case kDNSServiceErr_PollingMode:
            return "PollingMode";

        case kDNSServiceErr_Timeout:
            return "Timeout";
    }
}

struct mDNSReply
{
    string name, regtype, domain;
};

struct mDNSResolve
{
    string fullName;
    uint16_t port;
};

#define CHECKED(err)                                                                                                                                           \
    if ((err) != kDNSServiceErr_NoError)                                                                                                                       \
        throw SnapException(BonjourGetError(err) + ":" + to_string(__LINE__));

void runService(const DNSServiceHandle& service)
{
    if (!*service)
        return;

    auto socket = DNSServiceRefSockFD(*service);
    fd_set set;
    FD_ZERO(&set);
    FD_SET(socket, &set);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    while (select(FD_SETSIZE, &set, NULL, NULL, &timeout))
    {
        CHECKED(DNSServiceProcessResult(*service));
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
    }
}

bool BrowseBonjour::browse(const string& serviceName, mDNSResult& result, int /*timeout*/)
{
    result.valid = false;
    // Discover
    deque<mDNSReply> replyCollection;
    {
        DNSServiceHandle service(new DNSServiceRef(NULL));
        CHECKED(DNSServiceBrowse(
            service.get(), 0, 0, serviceName.c_str(), "local.",
            [](DNSServiceRef /*service*/, DNSServiceFlags /*flags*/, uint32_t /*interfaceIndex*/, DNSServiceErrorType errorCode, const char* serviceName,
               const char* regtype, const char* replyDomain, void* context)
            {
            auto replyCollection = static_cast<deque<mDNSReply>*>(context);

            CHECKED(errorCode);
            replyCollection->push_back(mDNSReply{string(serviceName), string(regtype), string(replyDomain)});
            },
            &replyCollection));

        runService(service);
    }

    // Resolve
    deque<mDNSResolve> resolveCollection;
    {
        DNSServiceHandle service(new DNSServiceRef(NULL));
        for (auto& reply : replyCollection)
            CHECKED(DNSServiceResolve(
                service.get(), 0, 0, reply.name.c_str(), reply.regtype.c_str(), reply.domain.c_str(),
                [](DNSServiceRef /*service*/, DNSServiceFlags /*flags*/, uint32_t /*interfaceIndex*/, DNSServiceErrorType errorCode, const char* /*fullName*/,
                   const char* hosttarget, uint16_t port, uint16_t /*txtLen*/, const unsigned char* /*txtRecord*/, void* context)
                {
                auto resultCollection = static_cast<deque<mDNSResolve>*>(context);

                CHECKED(errorCode);
                resultCollection->push_back(mDNSResolve{string(hosttarget), ntohs(port)});
                },
                &resolveCollection));

        runService(service);
    }

    // DNS/mDNS Resolve
    deque<mDNSResult> resultCollection(resolveCollection.size(), mDNSResult{});
    {
        DNSServiceHandle service(new DNSServiceRef(NULL));
        unsigned i = 0;
        for (auto& resolve : resolveCollection)
        {
            resultCollection[i].port = resolve.port;
            CHECKED(DNSServiceGetAddrInfo(
                service.get(), kDNSServiceFlagsLongLivedQuery, 0, kDNSServiceProtocol_IPv4, resolve.fullName.c_str(),
                [](DNSServiceRef /*service*/, DNSServiceFlags /*flags*/, uint32_t interfaceIndex, DNSServiceErrorType /*errorCode*/, const char* hostname,
                   const sockaddr* address, uint32_t /*ttl*/, void* context)
                {
                auto result = static_cast<mDNSResult*>(context);

                result->host = string(hostname);
                result->ip_version = (address->sa_family == AF_INET) ? (IPVersion::IPv4) : (IPVersion::IPv6);
                result->iface_idx = static_cast<int>(interfaceIndex);

                char hostIP[NI_MAXHOST];
                char hostService[NI_MAXSERV];
                if (getnameinfo(address, sizeof(*address), hostIP, sizeof(hostIP), hostService, sizeof(hostService), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
                    result->ip = string(hostIP);
                else
                    return;
                result->valid = true;
                },
                &resultCollection[i++]));
        }
        runService(service);
    }

    resultCollection.erase(std::remove_if(resultCollection.begin(), resultCollection.end(), [](const mDNSResult& res) { return res.ip.empty(); }),
                           resultCollection.end());

    if (resultCollection.empty())
        return false;

    if (resultCollection.size() > 1)
        LOG(NOTICE, LOG_TAG) << "Multiple servers found.  Using first" << endl;

    result = resultCollection.front();

    return true;
}

#undef CHECKED
