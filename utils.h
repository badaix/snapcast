#ifndef UTILS_H
#define UTILS_H

#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>


// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}


static std::string getMacAddress()
{
	std::ifstream t("/sys/class/net/eth0/address");
	std::string str((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());
	return trim(str);
}


std::vector<std::string> split(const std::string& str)
{
	std::istringstream iss(str);
	std::vector<std::string> splitStr;
	std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), std::back_inserter<std::vector<std::string> >(splitStr));
	return splitStr;
}




#endif


