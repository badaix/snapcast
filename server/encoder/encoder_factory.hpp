#ifndef ENCODER_FACTORY_H
#define ENCODER_FACTORY_H

#include "encoder.hpp"
#include <memory>
#include <string>

namespace encoder
{

class EncoderFactory
{
public:
    //	EncoderFactory(const std::string& codecSettings);
    std::unique_ptr<Encoder> createEncoder(const std::string& codecSettings) const;
};

} // namespace encoder

#endif
