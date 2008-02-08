#ifndef OBJ_H
#define OBJ_H
#include "adt_cvutils.hpp"
#include <string>
#include <vector>
#include <map>
#include <istream>
#include <boost/thread/once.hpp>
#include <osg/ref_ptr>
#include <osg/Referenced>
#include <osg/Vec2>
#include <osg/Vec3>
#include <osg/Vec4>

#include <osgDB/ReaderWriter>
#include <osg/Geometry>
#include <osg/StateSet>
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/Texture2DArray>
#include <osg/Texture3D>
#include <osg/TexGen>
#include <osg/TexMat>

#include <osgDB/Registry>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <boost/thread/mutex.hpp>
#include <osg/Notify>
#include <osg/Node>
#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/Version>
#include "GraphicsWindowCarbon.h"
#include "PixelBufferCarbon.h"
#include "auv_mesh.hpp"
#include "auv_stereo_geometry.hpp"
#include "auv_ransac_plane.hpp"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "auv_clipped_texture_atlas.hpp"
#include "adt_hist.hpp"
#include "auv_mesh_io.hpp"
#include "Depth.h"
#include "auv_texture_utils.hpp"
using namespace auv_data_tools;
using namespace std;
using namespace libsnapper;
using namespace libpolyp;


#include <osgViewer/Viewer>
typedef  boost::function<bool( int ,int)> VerboseMeshFunc;
extern MyGraphicsContext *mgc;


#define MAX_TEX_UNITS 8

// collect all the data relavent to a particular osg::Geometry being created.
struct GeometryCollection
{
    GeometryCollection():
        _numPrimitives(0),
        _numPrimitivesWithTexCoords(0),
        _numPoints(0),
        _texturesActive(false),
        _vertices(osg::Vec3Array::iterator()),
        _texcoords(osg::Vec2Array::iterator()),
        _coordCount(0),
        _geom(0) {}
  
  int                         _numPrimitives;
  int                         _numPrimitivesWithTexCoords;
  int                         _numPoints;
  bool                        _texturesActive;
  osg::Vec3Array::iterator    _vertices;
  osg::Vec4Array::iterator    _colors;
  bool                        _colorsActive;
  osg::Vec2Array::iterator    _texcoords;
  osg::Vec2Array *    _texcoordArray;
  osg::Vec3Array::iterator    _texcoordsTexArray[NUM_TEX_BLEND_COORDS];
  osg::Vec2Array::iterator    _texcoordsPlane;
  int                         _coordCount;
  osg::Geometry*              _geom;
  bool _planeTexValid;
  osg::BoundingBox            _texLimits;


};
enum {IVE_OUT,OSG_OUT,THREEDS_OUT};

typedef std::map<int,GeometryCollection* > MaterialToGeometryCollectionMap;
typedef std::map<int,string> MaterialToIDMap;


class OSGExporter 
{
public:
  OSGExporter(string prefixdir="mesh/",bool tex_saved=true,bool compress_tex=false,int num_threads=1,int verbose=0,bool hardware_compress=true,bool tex_array_blend=false,bool do_novelty=false,string basedir="",bool usePlaneDist=false): prefixdir(prefixdir),tex_saved(tex_saved),compress_tex(compress_tex),num_threads(num_threads),verbose(verbose),_hardware_compress(hardware_compress),_tex_array_blend(tex_array_blend),gpuNovelty(false),computeHists(do_novelty),basedir(basedir),usePlaneDist(usePlaneDist) {state=NULL;
    do_atlas=false;
    _planeTexSize=32;
    context=NULL;
  
    //    internalFormatMode=osg::Texture::USE_IMAGE_DATA_FORMAT;
    if(compress_tex && _hardware_compress){
      
      if(verbose)
	printf("Compressing Textures\n");
      if(!mgc)
	mgc=new MyGraphicsContext();
      context=mgc;
      
      if(!context){
	printf("Can't use OPENGL without valid context");
	exit(0);
      }

      

      //internalFormatMode=osg::Texture::USE_S3TC_DXT1_COMPRESSION; 

    }
  }
  osg::Image *LoadResizeSave(string filename,string outname,bool save,int tex_size);
 
  bool outputModelOSG(char *out_name,  osg::ref_ptr<osg::Geode> *group);
~OSGExporter();

  std::map<string,IplImage *> tex_image_cache;
  std::map<string,osg::Image *> compressed_img_cache;
  osg::Image *getCachedCompressedImage(string name,int size);
  osg::ref_ptr<osg::Image>cacheCompressedImage(IplImage *img,string name,int tex_size);
  bool convertGtsSurfListToGeometryTexArray(GtsSurface *s, map<int,string> textures,ClippingMap *cm,int tex_size,osg::ref_ptr<osg::Geode>*group,VerboseMeshFunc vmcallback,float *zrange);
  bool convertGtsSurfListToGeometry(GtsSurface *s, std::map<int,string> textures,ClippingMap *cm,int tex_size, osg::ref_ptr<osg::Geode >* group,vector<Plane3D> planes,vector<TriMesh::BBox> bounds,VerboseMeshFunc vmcallback=NULL,float *zrange=NULL) ;  
  
  bool Export3DS(GtsSurface *s,const char *c3DSFile,map<int,string> material_names,int tex_size,VerboseMeshFunc vmcallback=NULL);

protected:
  
  osg::ref_ptr<osg::State> state;
  std::vector<GtsBBox *> bboxes_all;;
  void compress(osg::Texture2D* texture2D,osg::Texture::InternalFormatMode internalFormatMode=osg::Texture::USE_S3TC_DXT1_COMPRESSION );
  //osg::Texture::InternalFormatMode internalFormatMode;
  MyGraphicsContext *context;
  bool ive_out;
  
  string prefixdir;
  bool tex_saved;
  int _tex_size;
  bool compress_tex;
  bool do_atlas;
  int num_threads;
  int verbose;
  bool _hardware_compress;
  bool _tex_array_blend;
  bool gpuNovelty;
  bool computeHists;
  string basedir;
  bool usePlaneDist;
  vector<osg::ref_ptr<osg::Texture2D> > osg_tex_ptrs;
  vector<osg::ref_ptr<osg::Texture2DArray> > osg_tex_arr_ptrs;
  map<int,osg::Texture *> osg_tex_map;

  
  
  void  calcHists( MaterialToGeometryCollectionMap &mtgcm, map<int,string> textures, Hist_Calc &histCalc);
  void addNoveltyTextures( MaterialToGeometryCollectionMap &mtgcm, map<int,string> textures, Hist_Calc &histCalc,CvHistogram *hist);


  int _planeTexSize;
  
};


void gen_mesh_tex_coord(GtsSurface *s ,Camera_Calib *calib, std::map<int,GtsMatrix *> back_trans,GNode *bboxTree,int tex_size,int num_threads,int verbose=0,int blend=0);

void bin_face_mat_osg (T_Face * f, gpointer * data);

extern std::vector<GtsBBox *> bboxes_all;;
#endif
