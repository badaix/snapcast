#ifndef COMPAT_H
#define COMPAT_H


#include <string>

#ifdef NO_CPP11_STRING
#include <sstream>
#include <cstdlib>
#endif


namespace cpt
{
	template<typename T>
	static std::string to_string(const T& t)
	{
	#ifdef NO_CPP11_STRING
		std::stringstream ss;
		ss << t;
		return ss.str();
	#else
		return std::to_string(t);
	#endif
	}

	static long stoul(const std::string& s)
	{
	#ifdef NO_CPP11_STRING
		return atol(s.c_str());
	#else
		return std::stoul(s);
	#endif
	}

	static int stoi(const std::string& str)
	{
	#ifdef NO_CPP11_STRING
		return strtol(str.c_str(), 0, 10);
	#else
		return std::stoi(str);
	#endif
	}

	static double stod(const std::string& str)
	{
	#ifdef NO_CPP11_STRING
		return strtod(str.c_str(), NULL);
	#else
		return std::stod(str.c_str());
	#endif
	}

	static long double strtold(const char* str, char** endptr)
	{
	#ifdef NO_CPP11_STRING
		return strtod(str, endptr);
	#else
		return std::strtold(str, endptr);
	#endif
	}

	static float strtof(const char* str, char** endptr)
	{
	#ifdef NO_CPP11_STRING
		return (float)strtod(str, endptr);
	#else
		return std::strtof(str, endptr);
	#endif
	}
}


#endif

