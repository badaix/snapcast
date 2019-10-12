#ifndef ENCODER_FACTORY_H
#define ENCODER_FACTORY_H

#include "encoder.hpp"
#include <string>

class EncoderFactory
{
public:
    //	EncoderFactory(const std::string& codecSettings);
    Encoder* createEncoder(const std::string& codecSettings) const;
};


#endif
