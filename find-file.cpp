#include "DiskLoader.h"
#include "find-file.hpp"
#include "jpeg-parser.hpp"
#include "raw_extract.hpp"

#include <sys/mman.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <algorithm>
#include <string>
#include <iostream>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <ostream>
#include <iomanip>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <map>
#include <boost/iostreams/stream.hpp>
#include <boost/function.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace fournseek {
  const uint64_t maxCrawlSize = 10* 1000 * 1000 ;

  DataExtractor::DataExtractor(const std::string &filePrefix)
  {

  }

  DataExtractor::~DataExtractor()
  {

  }
}

// hash function.
// TODO profile perf overhead of using boost function 
// the general rabin karp matching algorithm needs a real hash function
// like murmurhash or the like
boost::function<uint64_t(const unsigned char *)> hash_func;

/** Simple and stupid hash function that doesn't hash anything. 
 *  Currently, return 32 bits of the buffer given in buffer.
 *  The 32bits limit comes from the JPEG markers that's not wider. 
 *  */
uint64_t hash32 (const unsigned char * buffer)
{
  //not legal on all architecture, not portable and not standard compliant
  return *((unsigned int*) buffer); //32 bits only for prototyping
}

uint64_t hash64 (const unsigned char * buffer)
{
  //not legal on all architecture, not portable and not standard compliant
  return *((uint64_t *) buffer);
}

bool extractBufferToFile(const unsigned char * buffer, size_t size,
                         unsigned int fileIndex, const std::string& prefix)
{
    std::stringstream filename;
    filename << prefix << fileIndex << ".jpg";
    boost::iostreams::mapped_file_sink mf;
    boost::iostreams::mapped_file_params params;
    params.new_file_size = size;
    params.path = filename.str();
    
    std::cout << "Writing buffer to file " << size << " " << filename.str() << std::endl;
    mf.open( params ) ;
    std::copy(buffer, buffer+size, mf.data());
    mf.close();
    return true;
  //TODO no error handling
}


/** Adds a 64bits pattern for start of file byte sequences detection. */
void addPatternToMap(std::map<uint64_t,int>& map, const std::basic_string<unsigned char> &buffer)
{
   map[ (uint64_t) hash_func(&buffer[0])]=1;
}

/** 
 * Use a file to extract start-of-file pattern from it and then use it
 * as start-of-file pattern marker for our analysis.
 * We only use the first bytes of the file.
 * */
void addFileToMap(std::map<uint64_t,int> &map, const std::string& filename)
{
  namespace po = boost::program_options;
  namespace fs = boost::filesystem;
  namespace io = boost::iostreams;
/*
  io::mapped_file_source mf;
  mf.open( io::mapped_file_params(filename) );
  const unsigned char * b = reinterpret_cast<const unsigned char * > (mf.data());
  uint64_t hash = hashme( b );
  mf.close();
  */

}

/**
 *  Main program entry point. Processes CLI arguments
 *
 * */
int main(int argc, char * argv[])
{
  namespace po = boost::program_options;
  namespace fs = boost::filesystem;
  namespace io = boost::iostreams;
  using namespace fournseek;

  //map containing hashes
  std::map<uint64_t, int > hashes;

  // Declare the supported options.
  po::options_description desc("Options");
  desc.add_options()
    ("help", "This help message")
    ("image", po::value< std::string >(),  "path of image disk  to search in")
    ("start", po::value<unsigned int>(),"Start offset of search" )
    ("prefix",po::value<std::string>()->default_value("output/file-"), "Prefix to use for extracting files. Can contain a directory '/'). Defaults to 'output/file-'.")
    ("pattern", po::value<std::string>(),"Pattern to search for, in ASCII")
    ("parser", po::value<std::string>()->default_value("jpeg"),"Parser to use to extract blobs. Valid values : jpeg, none.")
    ("end", po::value<unsigned int>(),"End offset of search" )
  ;


  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch(std::exception& e ) {
    desc.print( std::cout );
    return -1;
  }

  if (!vm.count("image") ) {
    std::cerr << "[ERROR] Missing argument 'image' ! \n" ;
    desc.print( std::cout );
    return -1;
  }

  size_t start = 0;
  if (vm.count("start") > 0 ) {
    start = vm["start"].as<unsigned int>();
  }


  const std::string filePrefix = vm["prefix"].as<std::string>();
  DataExtractor * extractor = NULL;

  std::string parser;

  //check if user wants raw extractor
  if( vm.count("parser")) {
    parser = vm["parser"].as<std::string>();
  }


  hash_func = &hash32;
  if (vm.count("pattern")  ) {
     std::string pattern = vm["pattern"].as<std::string>();
     if (parser != "raw") {
       std::cout << "[ERROR] If using a user-defined pattern, you must use the raw parser. "
                 << "\nSuggestion : try adding --parser=raw to the command line"
                 << std::endl;
       return -4;
     }

     if( pattern.size() <= 4) {
        std::cout << "Pattern too small, at least 4 bytes required. Aborting.\n";
        return -3; 
     } 
     if( pattern.size() > 4) { 
        std::cout << "Using 64 bits hashing." << std::endl;
        hash_func = &hash64;
     }
     std::cout << "Using raw extractor and pattern to find and extract blobs.\n";
     extractor = new RawExtract( Pattern((unsigned char *)pattern.c_str(), pattern.size()) );

  //   addPatternToMap( hashes, Pattern((unsigned char *)pattern.c_str(), pattern.size()) );
  }

  //default extractor : jpeg
  if (!extractor ) {
    std::cout << "Using default extractor : Jpeg\n";
    extractor = new JpegParser( filePrefix );
  }

  assert( extractor != NULL);

  auto listofSOP = extractor->getSOFPatterns();
  std::cout << "Extractor provided " << listofSOP.size() << " patterns for start of file recognition\n";

  auto it = listofSOP.begin();
  auto const itEnd = listofSOP.end();
  for ( ; it != itEnd; ++it) {
    addPatternToMap( hashes, *it );
  }

  const std::string imageDiskPath = vm["image"].as<std::string>( ) ;

#define MMAP_BOOST 
//#define MMAP_POSIX
//#define RINGBUFFER

  if (!fs::is_regular_file(imageDiskPath)) {
    std::cerr << "File " << imageDiskPath << " is not a regular file "
              << std::endl;
#if defined(MMAP_BOOST)
   // when using boost iostream mapped files, we can't use device files. 
   // The raw posix API can evidently handle this case.
   return -2;
#endif 
  }

  //our main file buffer
  const unsigned char * buffer = NULL;
  size_t mappedFileSize = 0;
  try {
    uint64_t hash = 0 ;

#if defined(MMAP_POSIX)
    int file_descriptor =  open (imageDiskPath.c_str(), O_RDONLY);
    if ( file_descriptor == -1) {
      std::cout << "Can't open file " << imageDiskPath << std::endl;
      exit(1);
    }
    if (ioctl(file_descriptor, BLKGETSIZE64, &mappedFileSize)) {
      std::cerr << "Can't get device size !\n";
      exit(1);
    }

    void * mapping = mmap(NULL, mappedFileSize, PROT_READ, MAP_PRIVATE, file_descriptor, 0);
    if( mapping == MAP_FAILED) {
      std::cerr << "Can't mmap the file \n";
      exit(1);
    }
    buffer = (const unsigned char * ) mapping;
    std::cout << "mapped file size: " << mappedFileSize << " bytes"<< std::endl;
#elif defined(MMAP_BOOST)
    io::mapped_file_source mappedSource;
    mappedSource.open( io::mapped_file_params(imageDiskPath) );
    buffer = reinterpret_cast<const unsigned char * > (mappedSource.data());
    std::cout << "mapped file size : " << mappedSource.size()<< " bytes"<< std::endl;
    mappedFileSize = mappedSource.size();
#endif

    if (mappedFileSize < sizeof(uint64_t)) {
      std::cerr << "Error while opening/mapping file : file too short or error " << std::endl;
      return -1;
    }
    auto map_end = hashes.end();

    std::cout << "Using extractor : " << extractor->getName() << std::endl;
    size_t end = mappedFileSize;
    if (vm.count("end") > 0 ) {
      end = std::min((size_t) vm["end"].as<unsigned int>(), end - sizeof(uint64_t));
    }

    //enable auto flush for '.' to print
    std::cout.setf( std::ios_base::unitbuf );
    std::cout << "Starting search at offset " << std::dec << start << std::endl;
    for(size_t index = start;  index < end; ++index)
    {
      if ( index % 1000000 == 0) //every MB
      {
        std::cout << "\b\b\b\b\b\b\b\b\b\b\b"; //will work properly until 10 TB
        std::cout << std::dec << std::setw(8) << (index/1000000) << " MB";
      }

      hash = hash_func( buffer + index);
      if ( hashes.find(hash) != map_end ) {
        //TODO check we really found the pattern, to avoid false positive
        std::cout << "\nFound start of file pattern at offset "
                  << std::dec << index << std::endl;

        size_t extractSizeBefore = 0;
        size_t extractSizeAfter = 0;

        if (!extractor->checkData( buffer+index, mappedFileSize-index-1, extractSizeAfter, extractSizeBefore)) {
          std::cout << "We didn't found a valid file at this offset (" 
                    << std::dec << index << ")" << std::endl;
        } else {

          uint64_t startOffset = std::max( int64_t(index) - int64_t(extractSizeBefore), int64_t(0));
          uint64_t endOffset = std::min(index+ extractSizeAfter, end);
          if( !extractBufferToFile(buffer+startOffset, endOffset - startOffset, index, filePrefix)) {
          ///error
          }
          //TODO
          std::cout << "Extracted from offset "<< std::dec << startOffset <<
                       " to offset " << endOffset << " (" << (endOffset - startOffset)
                    << " bytes, due to pattern match at offset " << index << "). File id : "
                    << index << "." << std::endl;
        }
        //TODO ERROR index has been modified when before > 0
        index += extractSizeAfter;
      }
    }
#if defined(MMAP_BOOST)
    mappedSource.close();
#endif
  } catch( fs::filesystem_error& fse ) {
    std::cerr << "[ERROR] " << fse.what() << std::endl;
  }

  return 0;
}

