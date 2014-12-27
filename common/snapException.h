#ifndef SNAP_EXCEPTION_H
#define SNAP_EXCEPTION_H

#include <exception>
#include <string>
#include <cstring>        // std::strlen, std::strcpy

// text_exception uses a dynamically-allocated internal c-string for what():
class SnapException : public std::exception {
  char* text_;
public:
	SnapException(const char* text) 
	{
		text_ = new char[std::strlen(text)]; 
		std::strcpy(text_, text);
	}

	SnapException(const SnapException& e) 
	{
		text_ = new char[std::strlen(e.text_)]; 
		std::strcpy(text_, e.text_);
	}

	virtual ~SnapException() throw() 
	{
		delete[] text_;
	}

	virtual const char* what() const noexcept 
	{
		return text_;
	}
};


#endif


