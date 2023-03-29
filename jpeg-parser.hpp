#include "find-file.hpp"

namespace fournseek {

/** Defines an extractor: given a pre-match, this */
class JpegParser : public DataExtractor {
  public:
    JpegParser(const std::string& filePrefix );
    virtual ~JpegParser();
    virtual bool checkData( const unsigned char * buffer, size_t windowMax, 
                    size_t &sizeToExtractAfter, size_t &sizeToExtractBefore);
    virtual std::string getName() const {
      return u8"Jpeg files";
    }
    


    virtual std::list< std::basic_string<unsigned char> > getSOFPatterns();
};

}
