#include "browseBonjour.h"

#include <iostream>
#include <deque>
#include <sys/socket.h>

#include "common/snapException.h"

using namespace std;

BrowseBonjour::BrowseBonjour()
{
}

BrowseBonjour::~BrowseBonjour()
{
}

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

#define CHECKED(err) if((err)!=kDNSServiceErr_NoError)throw SnapException(BonjourGetError(err));

void BrowseBonjour::BonjourResolve(DNSServiceRef service,
																	 DNSServiceFlags flags,
																	 uint32_t interfaceIndex,
																	 DNSServiceErrorType errorCode,
																	 const char* fullName,
																	 const char* hosttarget,
																	 uint16_t port,
																	 uint16_t txtLen,
																	 const unsigned char* txtRecord,
																	 void* context)
{
	auto resultCollection = static_cast<deque<mDNSResolve>*>(context);

	CHECKED(errorCode);
	resultCollection->push_back(mDNSResolve { string(fullName), port });
}

void BrowseBonjour::BonjourReply(DNSServiceRef service,
																 DNSServiceFlags flags,
																 uint32_t interfaceIndex,
																 DNSServiceErrorType errorCode,
																 const char* serviceName,
																 const char* regtype,
																 const char* replyDomain,
																 void* context)
{
	auto replyCollection = static_cast<deque<mDNSReply>*>(context);
	
	CHECKED(errorCode);
	replyCollection->push_back(mDNSReply { string(serviceName), string(regtype), string(replyDomain) });
}

void runService(const DNSServiceRef& service, timeval& timeout)
{
	auto socket = DNSServiceRefSockFD(service);
	fd_set set;
	FD_ZERO(&set);
	FD_SET(socket, &set);
	
	while (select(FD_SETSIZE, &set, NULL, NULL, &timeout))
		CHECKED(DNSServiceProcessResult(service));
}

bool BrowseBonjour::browse(const string& serviceName, mDNSResult& result, int timeout)
{
  result.valid_ = false;
	
	timeval timeoutVal;
	timeoutVal.tv_sec = 5;
	timeoutVal.tv_usec = 0;
	
	// Discover
	deque<mDNSReply> replyCollection;
	{
		DNSServiceRef service = NULL;
		CHECKED(DNSServiceBrowse(&service, 0, 0, "_snapcast._tcp", "local.", BrowseBonjour::BonjourReply, &replyCollection));

		runService(service, timeoutVal);
		
		DNSServiceRefDeallocate(service); // TODO wrap RAII
	}
	
	timeoutVal.tv_sec = 5;
	timeoutVal.tv_usec = 0;

	// Results
	deque<mDNSResolve> resolveCollection;
	{
		DNSServiceRef service = NULL;
		for (auto& reply : replyCollection)
			CHECKED(DNSServiceResolve(&service, 0, 0, reply.name.c_str(), reply.regtype.c_str(), reply.domain.c_str(), BrowseBonjour::BonjourResolve, &resolveCollection));

		runService(service, timeoutVal);
		
		DNSServiceRefDeallocate(service); // TODO wrap RAII
	}

	// DNS/mDNS Resolve
	deque<mDNSResult> resultCollection;
	{
		DNSServiceRef service = NULL;
		for (auto& resolve : resolveCollection)
		{
			auto port = resolve.port;
			CHECKED(DNSServiceGetAddrInfo(&service, 0, 0, 0, resolve.fullName,
																		[port](DNSServiceRef service,
																					 DNSServiceFlags flags,
																					 uint32_t interface_index,
																					 DNSServiceErrorType errorCode,
																					 const char* hostname,
																					 const sockaddr* address,
																					 uint32_t ttl,
																					 void* context)
																		{
																		}, &resultCollection));
		}
		
		runService(service, timeoutVal);
		
		DNSServiceRefDeallocate(service); // TODO wrap RAII
	}

	cout << replyCollection.size() << endl;
	cout << resultCollection.size() << endl;
	for (auto& result : resultCollection)
		cout << result.host_ << endl;
	
	return true;
}

#undef CHECKED
