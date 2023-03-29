#pragma once
#include "find-file.hpp"
#include <vector>
namespace fournseek {

  class RawExtract: public DataExtractor
  {
    public:
      RawExtract(const std::basic_string<unsigned char >& pattern);
      virtual ~RawExtract();
      virtual bool checkData(const unsigned char *buffer, size_t windowMax,
                        size_t &sizeToExtractAfter, size_t &sizeToExtractBefore);
      virtual std::string getName() const {
        return "Raw zone";
      }

      virtual std::list< std::basic_string<unsigned char> > getSOFPatterns();
    protected:
      std::basic_string<unsigned char> _pattern;
  };
}
