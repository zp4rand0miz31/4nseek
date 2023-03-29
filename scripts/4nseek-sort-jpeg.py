#!/usr/bin/env python
# Quick usage: 
#  4nseek-sort.py source_directory destination_directory 
# 1. filter options : 
#         --min-size : used to filter thumbnails and so
#         --camera : used to include a camera, use many options if 
#                    several cameras are required in the list. No option will result 
#                    in all camera models to be accepted.
#         --image-min-size : 
#    
# 2. sorting options : 
#         --move : move files (else, don't do anything)
#         --dest : format %C %D %T  (python interpretable string)  OUT/YEAR/MONTH-letter/jour
#         -- TODO : what do we do with duplicates files ? take the bigger one ? check with exposure time or other finer criteria ?
#         --group-by GPS (10 km precision), time 
# 3. summary : 
#       - report files seen by camera, avg(size), max(size), file_count
#       - min_date, max_date 
#       - a report : uniqueK|date|time|resolutionX|Y|size|filename|destfilename

import logging
import os
import os.path
import shutil
import sys
import exifread

source = sys.argv[1]
destination = sys.argv[2]
#TODO perform parameter analysis
while not os.path.exists(source):
    source = raw_input('Enter a valid source directory\n')

while not os.path.exists(destination):
    destination = raw_input('Enter a valid destination directory\n')

FORMAT = '%(asctime)-15s %(levelname)s %(message)s'
logging.basicConfig(format=FORMAT)
logger = logging.getLogger(__name__)
logger.debug("Start of program")
VERBOSE = 8 #verbose level is < debug  = 10
DUMP = 5 # dump all contents (exif)
logger.setLevel( VERBOSE ) 
logger.setLevel( DUMP ) 
logger.setLevel( logging.INFO )
logger.setLevel( VERBOSE) 
class FileInfo(object): 
    """ Stores meta data about a file, coming from the EXIF header or the 
    file itself.
    """

    def __init__(self, file_path, file_size, tags):
        """ Constructor. """
        self.file_size = file_size
        self.file_path = file_path 
        self.tags = tags
        self.destination_path = None

    def get_camera_model(self):
        """ Gets the camera model of this file."""
        if "Image Model" in self.tags:
            logger.log(VERBOSE,'Found camera model {0}'.format( self.tags["Image Model"] ))
	    return str(self.tags["Image Model"])
        return None
           
    def get_file_path(self): 
        """ Get file path. """
        return self.file_path

    def set_destination_path(self, dest): 
        self.destination_path = dest

    def get_destination_path(self): 
        """ Get the destination path of the file."""
        return self.destination_path

    def get_file_size(self):
        """ Returns the file size. """
        return self.file_size 
    
    def get_tags(self): 
        """ Gets EXIF tags. This is a low level interface. """
        return self.tags 

    def __get_key_if_exist(self, k, default): 
        """ Get a value from the EXIF tags, if it exists. Else, return default. """
        if k in self.tags: 
            return self.tags[k]
        return default 
 
    def get_image_width(self): 
        """ Get image width from the EXIF tags. """
        return self.__get_key_if_exist("EXIF ExifImageWidth",-1)
    
    def get_image_height(self): 
        """ Get image width from the EXIF tags. """
        return self.__get_key_if_exist("EXIF ExifImageLength",-1)
       
    def get_datetime(self):
        """ Get date time info from EXIF tags. """ 
        pass

    def get_fingerprint(self):
	""" Fingerprint ~ DATE_width_height 
        """        
        return self.get_properties_string("{datetime}_{w}_{h}")
    
    def get_fine_fingerprint(self):
	""" Fine Fingerprint ~ DATE_width_height_exposure_time_maxaperture
        """        
        return self.get_properties_string("{datetime}_{w}_{h}_{exposure}_{aperture}")
    
    def get_properties_string(self,pattern, **kwargs):
        h = self.__get_key_if_exist("EXIF ExifImageLength","")
        w = self.__get_key_if_exist("EXIF ExifImageWidth","")
        datetime = self.__get_key_if_exist("EXIF DateTimeOriginal","")
        max_aperture = self.__get_key_if_exist("MaxApertureValue","")
        exposure_time = self.__get_key_if_exist("EXIF ExposureTime","")
        datetime = str(datetime).replace(":","_").replace(" ","_")
        mapping = { 13 : 'h', 16: 'm'}
        
        s = list(datetime)
        if len(s) >= 17:
          for i in mapping:
             s[i] = mapping[i]
        datetime = "".join(s)
        year = "".join(s[0:4]) #FIXME this code ***assume *** the date string is valid.
        month = datetime[5:7] #TODO convert it to text 
        camera_model = self.get_camera_model() 
        return pattern.format(year=year,Month=month,month=month, datetime=datetime, camera=camera_model, h=h,w=w,exposure = exposure_time, aperture = max_aperture, **kwargs) 
  
    def get_csv_metadata(self):
        """ Get metadata formatted in CSV : .... ."""
        return self.get_properties_string("{uniqueK};{filepath};{filesize};{h};{w};{datetime};{aperture};{exposure};{camera}", 
                    uniqueK=self.get_fine_fingerprint(), filepath=self.get_file_path(), filesize=self.get_file_size() )
  
class FilteringOptions:
    """
    Modelizes user's choices such as filtering options and patterns for moving files. 
    """
    
    def __init__(self):
        self.cameras = set()
        self.require_camera = False
        self.size_min  = 0 #no size min by default
        self.image_min_x = 0
        self.image_min_y = 0
        self.move_files = False
	self.pattern = "{datetime}_{h}_{w}"
        self.sort_unreadable = True
        self.unreadable_target_dir = "no-exif-info"

    def add_camera(self, camera):
        """ Adds a Camera model to this filtering """
        logger.debug("Adding camera "+str(camera)+" to filter list")
        self.cameras.add( camera )
        
    def set_min_file_size(self, size_min):
        logger.debug("Setting min size to " +str(size_min)) 
        self.size_min = size_min

    def set_min_image_size(self, x, y): 
        """ Used in conjunction with set_min_file_size() to filter
            out buggy images (big file, small image). 
        """
        self.image_min_x = x
        self.image_min_y = y

    def set_sort_unreadable(self, value, target_directory):
        self.sort_unreadable = value 
        self.unreadable_target_dir = target_directory

    def set_require_camera(self, value): 
        """ Set the 'require camera' parameter : file 
            without a camera model will be rejected. 
            Disabled by default.
        """
        self.require_camera = value
    
    def accept(self, file_info):
        """ Do we include this file into the pool ? """
        #TODO : implement filtering by image size
        logger.log(VERBOSE,"Testing file:  " + file_info.get_file_path() )
        if self.size_min > 0 and file_info.get_file_size() < self.size_min:
            logger.log(VERBOSE,'rejecting file : {0} (size: {1}, min size: {2}'.format( file_info.get_file_path(), 
                                                                                  file_info.get_file_size(), 
                                                                                  self.size_min))   
            return False

        if not file_info.get_camera_model() and self.require_camera: 
            return False

        if file_info.get_camera_model() and len(self.cameras) > 0:
           match_camera = False
           for camera in self.cameras: 
               logger.log(VERBOSE, 'Model tested : {0} vs {1}'.format( camera, str(file_info.get_camera_model()) )) 
               if str(file_info.get_camera_model()).find( camera ) >= 0:
                   match_camera = True
                   continue
           # we didn't find any camera in this file metadata ? reject the file
           if not match_camera:
               logger.log(VERBOSE,'Rejecting file because camera model {0} didn\'t match!'.format( file_info.get_camera_model()))
               return False
 
        return True

    def set_target_pattern(self, pattern):
        """ Sets the pattern for file moves.
 
            Accepted variables name (in the string::format() format): 
              - camera : camera model 
              - h : height
              - w : width 
              - exposure : exposure of the picture
              - aperture : aperture of the picture
              - datetime : a date time string
              - filesize : the size of the file
              - filepath : the initial path of the file
        """ 
        self.pattern = pattern 
 
    def get_target_path(self, file_infos): 
        """ Get the target path according to the moving pattern. """
        return file_infos.get_properties_string( self.pattern )
    
    def set_move_files(self, value):
        """ Enable or disable move of file. """  
        self.move_files = value

    def get_move_files(self): 
        """ Whether we want to move files or not. """
        return self.move_files
        
class SortResult:
    """ Results of the sorting script. """

    def __init__(self):
        """ Constructor. """
        self.seen = 0
        self.accepted = 0 
        self.unreadable = 0
        self.cameras = dict() 
        self.uniqu = dict()
        self.photos = [] # this is a sequence of FileInfo 

    def notify_file_seen(self):
        """ Notification callback when a file is opened. """
        self.seen += 1
     
    def notify_file_unreadable(self, file_info):
        """ Notification callback when a file is unreadable by exifread(). """
        self.unreadable += 1

    def notify_file_accepted( self, file_info): 
        """ 
           Notification callback when a file is accepted in the 
           filtering layer.
        """
        self.accepted += 1
        self.photos += [ file_info ]
 
        # statistics on camera model
        if file_info.get_camera_model(): 
            self.cameras[ file_info.get_camera_model() ] = 1 + self.cameras.get(file_info.get_camera_model(), 0)
        else :
            self.cameras["unknown-camera-model"] = 1 + self.cameras.get("unknown-camera-model",0)
        
        # check duplicate

        logger.log(VERBOSE, file_info.get_fingerprint())
        #if self.unique.has_key( file_info
        fp = file_info.get_fine_fingerprint()
        if fp in self.uniqu:
           
           logger.info('File {0} has same fingerprint of previously seen file {1}'.format( file_info.get_file_path(), self.uniqu[fp] ))
               
        self.uniqu[fp] = file_info.get_file_path()
    def get_files_seen_count(self): 
        return self.seen
 
    def get_files_accepted_count(self):
        return self.accepted
 
    def get_unreadable_files_count(self):
        return self.unreadable

    def get_with_gps_count(self):
        return None 

    def get_cameras(self):
        return self.cameras  

    def dump_photos_db(self): 
        """ Dump all the accepted-photo database. """
        with open("4nseek-report.csv","w+") as f: 
            filter( lambda x: f.write(x.get_csv_metadata()+"\n"), self.photos)

def read_file_info( imageFileName ):
    """ 
    Read EXIF file info from filename.

    Returns : FileInfo object
    """
    with open(imageFileName,'rb') as f: 
        tags = exifread.process_file( f, details=False  )
        if not tags:
            return None 
	try:
            logger.log(DUMP, str(tags)) 
        except:
            print 'Exception whle printing tags of {0}'.format( imageFileName )
            pass
        file_size = os.path.getsize( imageFileName ) 
        return FileInfo( imageFileName, file_size, tags ) 

    return None
 
def walkTree(filtering, source, result): 
    """
    Walk the tree, parsing files.

    @param filtering a FilteringOption object to help choosing files
    @param source the source directory 
    @param result the result structure, containing statistics
    @return 
    """
    # walk the source directory 
    for root, dirs, files in os.walk(source, topdown=False):
        for file in files:
           extension = os.path.splitext(file)[1][1:].upper()
           imageFileName = root + "/" + str(file)
           file_info = read_file_info( imageFileName )
           result.notify_file_seen()
           if not file_info:
               logger.debug('File unreadable : {0}'.format( imageFileName))
               result.notify_file_unreadable( file_info )
               if filtering.sort_unreadable: #user wants unreadable to be sorted, somehow
                   target_directory = destination + "/"+ filtering.unreadable_target_dir
                   if not os.path.isdir( target_directory ) :
                       #create dir
                       os.makedirs( target_directory )
                   destinationPath = target_directory + "/" + str(file)
                   try:
                      os.link( imageFileName, destinationPath) 
                   except Exception as e: 
                      print "Exception during linking : {0} -> {1} ".format(imageFileName, destinationPath) + str(e)        
                   
               continue

           if not filtering.accept( file_info ):
               logger.debug('File ignored : {0}'.format( imageFileName ))
               continue
           # here, file has been accepted into the pool 
           result.notify_file_accepted( file_info ) 
           logger.debug('File included : {0}'.format( imageFileName ))
            
           if filtering.get_move_files():
               target_path = filtering.get_target_path( file_info )
               logger.log(VERBOSE, 'Will move file {0} to {1}'.format( 
                                 file_info.get_file_path(), 
                                 target_path))
	       destinationPath = destination+target_path
               logger.log(VERBOSE,"Destination path : " + destinationPath )
               target_directory = os.path.dirname( destinationPath )
               if not os.path.isdir( target_directory ) :
                   #create dir
                   os.makedirs( target_directory )
               if os.path.isfile( destinationPath ): 
                   target_duplicate_file_exists = True
                   target_index = 1
                   destPath = destinationPath + ".duplicate-{0}.jpg".format( target_index ) 
                   target_duplicate_file_exists = os.path.isfile( destPath ) 
                   while target_duplicate_file_exists:
                       target_index += 1
                       destPath = destinationPath + ".duplicate-{0}.jpg".format( target_index ) 
                       target_duplicate_file_exists = os.path.isfile( destPath ) 
                   destinationPath = destPath 
 
                   logger.warning("Destination file {0} already exist ! NOT COPIED : {1}".format(destinationPath,  file_info.get_file_path()))
               
               try :
                  os.link( file_info.get_file_path(), destinationPath) 
               except Exception as e: 
                  print "Exception during linking : {0} -> {1} ".format(file_info.get_file_path(), destinationPath) + str(e)        
        #if not os.dir 
#TODO provide multiple source directories           
           
#import pyexiv2
count = 0
DEBUG = True 
def getDestinationDir( date ):
    """ 
       Get the destination directory according to the date. 
    """ 
    s = str(date).replace(":","_")[0:10]
    return s[:4] + '/' + s[5:]

filter_ = FilteringOptions()
filter_.set_min_file_size( 200000 ) 
filter_.set_min_image_size( 300, 300)
#filter_.add_camera("KODAK")
#filter_.add_camera("GMC")
#filter_.add_camera("KODAK")
filter_.set_move_files(True)

#TODO implement this 
filter_.set_sort_unreadable(True,"no-exif-information/")
print "TODO : implment sort unreadable feature in a special repository" 

filter_.set_target_pattern("{year}/{Month}/{datetime}_{w}_{h}.jpg")
sort_result = SortResult()
 
walkTree(filter_, source, sort_result )

sort_result.dump_photos_db()
print "total number of photos : {0}" .format(sort_result.get_files_seen_count() )
print "total number of valid and included photos : {0}" .format(sort_result.get_files_accepted_count() )
print "unreadable photos       : {0}".format( sort_result.get_unreadable_files_count() )
#print "GPS       : {0}".format( sort_result.get_with_gps_count() )
print 'Camera models list : {0}'.format( sort_result.get_cameras() ) 
exit(1)

#	    shutil.copy2(os.path.join(root,file), destinationPath)

