#include "browseBonjour.h"

#include <future>
#include <experimental/optional>
#include <iostream>

#include "common/snapException.h"

using namespace std;
using namespace std::experimental;

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
	cout << serviceName << "," << regtype << "," << replyDomain << endl;
}

bool BrowseBonjour::browse(const string& serviceName, mDNSResult& result, int timeout)
{
	DNSServiceRef service = NULL;
	CHECKED(DNSServiceBrowse(&service, 0,	0, "_snapcast._tcp", "local", BrowseBonjour::BonjourReply, NULL));
	while (true)
	{
		CHECKED(DNSServiceProcessResult(service));
	}
	DNSServiceRefDeallocate(service);
	return true;
}

#undef CHECKED
