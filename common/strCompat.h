#ifndef COMPAT_H
#define COMPAT_H


#include <string>

#ifdef NO_CPP11_STRING
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <climits>
#include <stdexcept>
#include <cerrno>
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

	static long stoul(const std::string& str)
	{
	#ifdef NO_CPP11_STRING
		errno = 0;
		char *temp;
		long val = strtol(str.c_str(), &temp, 10);
		if (temp == str.c_str() || *temp != '\0')
			throw std::invalid_argument("stoi");
		if (((val == LONG_MIN) || (val == LONG_MAX)) && (errno == ERANGE))
			throw std::out_of_range("stoi");
		return val;
	#else
		return std::stoul(str);
	#endif
	}

	static int stoi(const std::string& str)
	{
	#ifdef NO_CPP11_STRING
		return cpt::stoul(str);
	#else
		return std::stoi(str);
	#endif
	}

	static double stod(const std::string& str)
	{
	#ifdef NO_CPP11_STRING
		errno = 0;
		char *temp;
		double val = strtod(str.c_str(), &temp);
		if (temp == str.c_str() || *temp != '\0')
			throw std::invalid_argument("stod");
		if ((val == HUGE_VAL) && (errno == ERANGE))
			throw std::out_of_range("stod");
		return val;
	#else
		return std::stod(str.c_str());
	#endif
	}

	static long double strtold(const char* str, char** endptr)
	{
	#ifdef NO_CPP11_STRING
		return cpt::stod(str);
	#else
		return std::strtold(str, endptr);
	#endif
	}

	static float strtof(const char* str, char** endptr)
	{
	#ifdef NO_CPP11_STRING
		return (float)cpt::stod(str);
	#else
		return std::strtof(str, endptr);
	#endif
	}
}


#endif

