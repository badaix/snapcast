#ifndef SNAP_EXCEPTION_H
#define SNAP_EXCEPTION_H

#include <exception>
#include <string>


struct snapException : std::exception {
	snapException(const std::string& what) noexcept
	{
		what_ = what;
	}

	const char* what() const noexcept 
	{
		return what_.c_str();
	}

private:
	std::string what_;
};


#endif


