#include "jpeg-parser.hpp" 

#include <iostream>
using namespace fournseek;

JpegParser::JpegParser(const std::string& prefix):
  DataExtractor( prefix ) 
{

}

JpegParser::~JpegParser() 
{

}

//JFIF markers, used to detected start of jpeg files
const unsigned char JFIFsig[4] = { 0xff, 0xd8, 0xff, 0xe0 };
const unsigned char JFIFsig2[4] = { 0xff, 0xd8, 0xff, 0xe1 };


std::list< std::basic_string<unsigned char> > JpegParser::getSOFPatterns()
{
  std::list< std::basic_string<unsigned char> > l;
  std::basic_string<unsigned char> sig1( JFIFsig, 4);
  std::basic_string<unsigned char> sig2( JFIFsig2, 4);

  l.push_back( sig1 );
  l.push_back( sig2 );

  return l;
}


  unsigned short readle16(const unsigned char * buffer) {
    return (buffer[0] << 8) + buffer[1];
  }


  /** Parses a probable Jpeg-buffer, looking for EOI and SOI sequences
   * @brief parseJpegBuffer
   * @param fileIndex
   * @param originaloffset
   * @param buffer
   * @param max
   * @return true if image has been detected (and extracted)
   */
  bool JpegParser::checkData(const unsigned char *buffer, size_t windowMax,
                             size_t &sizeToExtractAfter, size_t &sizeToExtractBefore)
  {
    size_t lastPosition = 0; //lastPosition of a EOI : usefull for extracting
    bool searchingEOI   = false; // are we searching a EOI after a SOS ?
    unsigned char last  = 0; //last valid marker found

    for (size_t offset  = 0; offset < windowMax; ++offset ) {

      if (offset > maxCrawlSize) { //no more than 10M
        LOGGER( "Stopped search because nothing found" << std::endl);
        return false;
      }

      //process EOI
      if (buffer[offset] == 0xff and buffer[offset+1] == 0xd8) {
        LOGGER("SOI @ " << offset << ", ");
        if ((last == 0xd9) and !searchingEOI) {
          LOGGER( "\nFound a contiguous SOI after EOI. This is also part of the image !" << std::endl);
        }
        last = buffer[offset+1];
        offset += 1; //+skip;
        continue;
      }

      //process special marker SOS (start of scan : no length of block, terminated by EOI)
      if (buffer[offset] == 0xff and buffer[offset+1] == 0xda) {
        LOGGER( "SOS @ " << offset << " searching EOI..., ");
        last = buffer[offset+1];   offset += 1; //+skip;
        searchingEOI = true;
        continue;
      }

      //process valid markers except EOI (d9)
      if (buffer[offset] == 0xff) {
        switch (buffer[offset+1]) {

          case 0xdc:
          case 0xc4:
          case 0xc0:
          case 0xe0:
          case 0xe1:
          case 0xe2:
          case 0xe3:
          case 0xe4: //APP4 => todo : handle appn
          case 0xdd:
          case 0xdb : {
            LOGGER( std::hex << unsigned(buffer[offset+1]) << std::dec << " @ " << offset << ", ");
            unsigned short skip = readle16(buffer +offset+2);
            LOGGER( "skipping : " << skip << " bytes,");
            last = buffer[offset+1];
            offset += 1+skip;
            continue;
          }
          default:;
        }
      }

      //found a EOI
      if (buffer[offset] == 0xff and buffer[offset+1] == 0xd9) {
        //we don't stop on a SOI because there can have several EOI in a image (the same
        // number as SOI. But we can found contiguous SOI after EOI (don't know the reason).
        LOGGER( "EOI @ " << offset << ",");
        last = buffer[offset+1];
        searchingEOI = false;
        offset += 1;
        lastPosition = offset+1;//this will become the last byte to extract
        continue;
      }

      //if we go here :
      //we didn't found a valid JFIF marker here

      if (!searchingEOI) { //and we were not looking for a EOI (after a SOS)
        //      if ( last == 0xd1 ) { //if last is EOI... :
        //        //decide to stop here ?
        //        LOGGER( "Stopping processing @ " << std::dec << offset<< " "<<std::endl) ;
        //        break;
        //      }
        //      //we expect a jfif marker
        LOGGER( "\n  Parsing error at " << offset << "(EOI not found during SOS????TODCHECK), ");
        LOGGER( std::hex << unsigned(buffer[offset]) << unsigned(buffer[offset+1]));
        unsigned short skip = readle16(buffer +offset+2);
        LOGGER( "skipping : " << skip << ", ");
        offset += 1+skip;
        break; //
      }
    } //main loop
    LOGGER("\n");
    //we extract til the last EOI seen to ensure some conformance
    if (last == 0xd9 && lastPosition > 0)  {
      sizeToExtractAfter = lastPosition;
      return true;
    }
    return false;
  }



