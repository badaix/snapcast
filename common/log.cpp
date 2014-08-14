#include "log.h"

Log::Log(std::string ident, int facility) {
    facility_ = facility;
    priority_ = LOG_DEBUG;
    strncpy(ident_, ident.c_str(), sizeof(ident_));
    ident_[sizeof(ident_)-1] = '\0';

    openlog(ident_, LOG_PID, facility_);
}

int Log::sync() {
    if (buffer_.length()) {
        syslog(priority_, buffer_.c_str());
        buffer_.erase();
        priority_ = LOG_DEBUG; // default to debug for each message
    }
    return 0;
}

int Log::overflow(int c) {
    if (c != EOF) {
        buffer_ += static_cast<char>(c);
    } else {
        sync();
    }
    return c;
}

std::ostream& operator<< (std::ostream& os, const LogPriority& log_priority) {
    static_cast<Log *>(os.rdbuf())->priority_ = (int)log_priority;
    return os;
}


