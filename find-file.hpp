#pragma once
#include <stdint.h>

#include <string>
#include <map>
#include <iosfwd>
#include <list>

//#define LOGGER(___x)
#define LOGGER(___x) { std::cout << ___x ;}

bool extractBufferToFile(const unsigned char * buffer, size_t size,
                         unsigned int fileIndex, const std::string& prefix);
uint64_t hash32 (const unsigned char * buffer);
uint64_t hash64 (const unsigned char * buffer);
unsigned short read16(const unsigned char * buffer);
void addPatternToMap(std::map<uint64_t,int>& map, const std::basic_string<unsigned char> &pattern);
void addFileToMap(std::map<uint64_t,int> &map, const std::string& filename);

namespace fournseek {

  typedef std::basic_string<unsigned char> Pattern;
  typedef std::list< Pattern > PatternList;

  extern const uint64_t maxCrawlSize;
  /** Defines an extractor: given a pre-match, this */
  class DataExtractor {
    public:
      DataExtractor(const std::string& filePrefix );
      virtual ~DataExtractor();

      virtual bool checkData( const unsigned char * buffer, size_t windowMax,
                      size_t &sizeToExtractAfter, size_t &sizeToExtractBefore) = 0;

      virtual std::string getName() const {
        return u8"Default extractor";
      }

      virtual std::list< std::basic_string<unsigned char> > getSOFPatterns()= 0 ;

  };

}
