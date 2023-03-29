#include "raw_extract.hpp"

using namespace fournseek;

RawExtract::~RawExtract()
{


}

RawExtract::RawExtract(const std::basic_string<unsigned char> &pattern) :
  DataExtractor(""),
  _pattern( pattern )
{

}

bool RawExtract::checkData(const unsigned char *buffer, size_t windowMax,
                           size_t &sizeToExtractAfter, size_t &sizeToExtractBefore)
{
  sizeToExtractAfter = maxCrawlSize;
  sizeToExtractBefore = 1000; //1kB of data . should be parametrable ?
  return true;
}


std::list< std::basic_string<unsigned char> > RawExtract::getSOFPatterns()
{
  std::list< std::basic_string<unsigned char> > l;
  l.push_back( _pattern );
  return l;
}
