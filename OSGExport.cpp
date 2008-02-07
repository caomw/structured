#include "OSGExport.h"
#include <osgUtil/TriStripVisitor>
#include <osgUtil/SmoothingVisitor>
#include <osgUtil/Optimizer>
#include <osg/GraphicsContext>
#include <osg/TexEnvCombine>
#include <osg/PolygonMode>
#include <osg/TextureRectangle>
#include <osgDB/WriteFile>
#include <osg/Point>
#include <osgUtil/Simplifier>
#include <osg/ShapeDrawable>
#include <cv.h>
#include <glib.h>
#include <highgui.h>
#include <boost/thread/thread.hpp>

#include <sys/stat.h>
#include "auv_texture_utils.hpp"
#include "auv_lod.hpp"
#include "auv_gts_utils.hpp"

using namespace libpolyp;
typedef struct _GHashNode      GHashNode;
using namespace libsnapper;
using namespace squish;
FILE *ffp;


MyGraphicsContext *mgc=NULL;
std::vector<GtsBBox *> bboxes_all;;
int texMargin=100;

MyGraphicsContext::MyGraphicsContext()
{
  osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
  traits->x = 0;
  traits->y = 0;
  traits->width = 1;
  traits->height = 1;
  traits->windowDecoration = false;
  traits->doubleBuffer = false;
  traits->sharedContext = 0;
  traits->pbuffer = false;
#ifdef __APPLE__
  _gw= new osgViewer::GraphicsWindowCarbon(traits.get()); 

#else
  _gc = osg::GraphicsContext::createGraphicsContext(traits.get());
	  
#endif

  if (_gc.valid()) 
            
            
    {
      _gc->realize();
      _gc->makeCurrent();
      std::cout<<"Realized window"<<std::endl;
				
    }else{
    printf("Can't realize window\n");
    exit(0);
  }
}
       
boost::mutex bfMutex;
//FILE *errFP;
int lastBP;
bool Export3DS(GtsSurface *s,const char *c3DSFile,vector<string> material_names)
#ifdef USE_LIB3DS 
  ;
#else
{printf("Not using lib3ds code cannot output 3ds\n");
  exit(0);
}
#endif
//#include <osgUtil/Tessellator>
// collect all the data relavent to a particular osg::Geometry being created.

void OSGExporter::compress(osg::Texture2D* texture2D, osg::Texture::InternalFormatMode internalFormatMode){
 
  if(!state)
    state = new osg::State;
  
  
  osg::ref_ptr<osg::Image> image = texture2D->getImage();
  if (image.valid() && 
      (image->getPixelFormat()==GL_RGB || image->getPixelFormat()==GL_RGBA) &&
      (image->s()>=32 && image->t()>=32)){
    //internalFormatMode=osg::Texture::USE_S3TC_DXT1_COMPRESSION;
    texture2D->setInternalFormatMode(internalFormatMode);
    
    // need to disable the unref after apply, other the image could go out of scope.
    bool unrefImageDataAfterApply = texture2D->getUnRefImageDataAfterApply();
    texture2D->setUnRefImageDataAfterApply(false);
   
    // get OpenGL driver to create texture from image.
    texture2D->apply(*state);
    
    // restore the original setting
    texture2D->setUnRefImageDataAfterApply(unrefImageDataAfterApply);

    image->readImageFromCurrentTexture(state->getContextID(),true);
 
    texture2D->setInternalFormatMode(osg::Texture::USE_IMAGE_DATA_FORMAT);
  }

}

 void bin_face_mat_osg (T_Face * f, gpointer * data){
  MaterialToGeometryCollectionMap *mtgcm=(MaterialToGeometryCollectionMap *)data[0];
  //uint uiFace =  *((guint *) data[1]);

  GeometryCollection& gc = (*mtgcm)[f->material];
  gc._numPoints += 3;
  gc._numPrimitives += 1;
  if (f->material >= 0) 
    gc._numPrimitivesWithTexCoords += 1;

  GUINT_TO_POINTER ((*((guint *) data[1]))++);
}

static void bin_face_all_osg (T_Face * f, gpointer * data){
  GeometryCollection& gc = *(GeometryCollection *)data[0];
  
  gc._numPoints += 3;
  gc._numPrimitives += 1;

  GUINT_TO_POINTER ((*((guint *) data[1]))++);
}


static void add_face_mat_osg (T_Face * f, gpointer * data){
 
  MaterialToGeometryCollectionMap *mtgcm=(MaterialToGeometryCollectionMap *)data[0];
  float *zrange = (float *)data[4];
  map<int,string> *textures = (map<int,string> *)data[3];
  ClippingMap *cm=(ClippingMap *)data[2];
  GeometryCollection& gc = (*mtgcm)[f->material];
  osg::BoundingBox &texLimits=(*cm)[osgDB::getSimpleFileName((*textures)[f->material])];
  int planeTexSize=(int)data[7];
  bool usePlaneDist=(int)data[8];
  osg::PrimitiveSet::Mode mode;
  
  mode = osg::PrimitiveSet::TRIANGLES;
  
  gc._geom->addPrimitiveSet(new osg::DrawArrays(mode,gc._coordCount,3));
  gc._coordCount += 3;
  TVertex * v1,* v2,* v3;
  gts_triangle_vertices(&GTS_FACE(f)->triangle,(GtsVertex **)& v1, 
			(GtsVertex **)&v2, (GtsVertex **)&v3);
 

  /* (*gc._vertices++).set(GTS_VERTEX(v1)->p.x,GTS_VERTEX(v1)->p.y,GTS_VERTEX(v1)->p.z);
     (*gc._vertices++).set(GTS_VERTEX(v2)->p.x,GTS_VERTEX(v2)->p.y,GTS_VERTEX(v2)->p.z);
     (*gc._vertices++).set(GTS_VERTEX(v3)->p.x,GTS_VERTEX(v3)->p.y,GTS_VERTEX(v3)->p.z); 
     // printf("%f %f %f\n",GTS_VERTEX(v3)->p.x,GTS_VERTEX(v3)->p.y,GTS_VERTEX(v3)->p.z);*/

 
  
 
  (*gc._vertices++).set(GTS_VERTEX(v1)->p.y,GTS_VERTEX(v1)->p.x,-GTS_VERTEX(v1)->p.z);
  (*gc._vertices++).set(GTS_VERTEX(v2)->p.y,GTS_VERTEX(v2)->p.x,-GTS_VERTEX(v2)->p.z);
  (*gc._vertices++).set(GTS_VERTEX(v3)->p.y,GTS_VERTEX(v3)->p.x,-GTS_VERTEX(v3)->p.z);

  if(gc._planeTexValid && usePlaneDist){
    if(v1->plane >= 0)
      (*gc._texcoordsPlane++).set(v1->plane %planeTexSize, v1->plane / planeTexSize);    
    else
      (*gc._texcoordsPlane++).set(-1.0,-1.0);    

 if(v2->plane >= 0)
      (*gc._texcoordsPlane++).set(v2->plane %planeTexSize, v2->plane / planeTexSize);    
    else
      (*gc._texcoordsPlane++).set(-1.0,-1.0);    

 if(v3->plane >= 0)
      (*gc._texcoordsPlane++).set(v3->plane %planeTexSize, v3->plane / planeTexSize);    
    else
      (*gc._texcoordsPlane++).set(-1.0,-1.0);    
    
  
  
}
 
  if (gc._texturesActive && f->material >= 0){


    texLimits.expandBy(v1->u,1-v1->v,0.0);
    texLimits.expandBy(v2->u,1-v2->v,0.0);
    texLimits.expandBy(v3->u,1-v3->v,0.0);

    
    (*gc._texcoords++).set(v1->u,1-v1->v);    
    (*gc._texcoords++).set(v2->u,1-v2->v); 
    (*gc._texcoords++).set(v3->u,1-v3->v); 

  }else
    gc._texturesActive=false;

  if(gc._colorsActive){
    if(!zrange){
      (*gc._colors++).set(0.5,0.5,0.5,0.0);
      (*gc._colors++).set(0.5,0.5,0.5,0.0);
      (*gc._colors++).set(0.5,0.5,0.5,0.0);
    }
    float range=zrange[1]-zrange[0];
      
    float r,g,b,val;
    val = ( GTS_VERTEX(v1)->p.z -zrange[0] )/range;    
    jet_color_map(val,r,g,b);
    (*gc._colors++).set(r,b,g,1.0);
    val = ( GTS_VERTEX(v2)->p.z -zrange[0] )/range;    
    jet_color_map(val,r,g,b);
    (*gc._colors++).set(r,b,g,1.0);
    val = ( GTS_VERTEX(v3)->p.z -zrange[0] )/range;    
    jet_color_map(val,r,g,b);
    (*gc._colors++).set(r,b,g,1.0);
  }
}

static void add_face_all_osg (T_Face * f, gpointer * data){
 
  GeometryCollection &gc=*(GeometryCollection  *)data[0];
  float *zrange = (float *)data[4];
  map<int,string> *textures = (map<int,string> *)data[3];
  ClippingMap *cm=(ClippingMap *)data[2];
  map<int,int> *texnum2arraynum=(map<int,int> *)data[5];
  osg::BoundingBox &texLimits=(*cm)[osgDB::getSimpleFileName((*textures)[f->material])];
  osg::PrimitiveSet::Mode mode;
  
  mode = osg::PrimitiveSet::TRIANGLES;
  
  gc._geom->addPrimitiveSet(new osg::DrawArrays(mode,gc._coordCount,3));
  gc._coordCount += 3;
  TVertex * v1,* v2,* v3;
  gts_triangle_vertices(&GTS_FACE(f)->triangle,(GtsVertex **)& v1, 
			(GtsVertex **)&v2, (GtsVertex **)&v3);
 


  (*gc._vertices++).set(GTS_VERTEX(v1)->p.y,GTS_VERTEX(v1)->p.x,-GTS_VERTEX(v1)->p.z);
  (*gc._vertices++).set(GTS_VERTEX(v2)->p.y,GTS_VERTEX(v2)->p.x,-GTS_VERTEX(v2)->p.z);
  (*gc._vertices++).set(GTS_VERTEX(v3)->p.y,GTS_VERTEX(v3)->p.x,-GTS_VERTEX(v3)->p.z);

  
  
  if (gc._texturesActive ){


    texLimits.expandBy(v1->u,1-v1->v,0.0);
    texLimits.expandBy(v2->u,1-v2->v,0.0);
    texLimits.expandBy(v3->u,1-v3->v,0.0);
    if( f->material >= 0){
      (*gc._texcoords++).set(v1->u,1-v1->v);    
      (*gc._texcoords++).set(v2->u,1-v2->v); 
      (*gc._texcoords++).set(v3->u,1-v3->v); 
    }

    for(int i=0; i< 4; i++){
      (*gc._texcoordsTexArray[i]++).set(v1->uB[i], 1 - v1->vB[i],(*texnum2arraynum)[f->materialB[i]]);
      (*gc._texcoordsTexArray[i]++).set(v2->uB[i], 1 - v2->vB[i],(*texnum2arraynum)[f->materialB[i]]); 
      (*gc._texcoordsTexArray[i]++).set(v3->uB[i], 1 - v3->vB[i],(*texnum2arraynum)[f->materialB[i]]); 
      //   printf("%d ",f->materialB[i]);
    }
    //printf("\n");
   
  }

  if(gc._colorsActive){
    if(!zrange){
      (*gc._colors++).set(0.5,0.5,0.5,0.0);
      (*gc._colors++).set(0.5,0.5,0.5,0.0);
      (*gc._colors++).set(0.5,0.5,0.5,0.0);
    }
    float range=zrange[1]-zrange[0];
      
    float r,g,b,val;
    val = ( GTS_VERTEX(v1)->p.z -zrange[0] )/range;    
    jet_color_map(val,r,g,b);
    (*gc._colors++).set(r,b,g,1.0);
    val = ( GTS_VERTEX(v2)->p.z -zrange[0] )/range;    
    jet_color_map(val,r,g,b);
    (*gc._colors++).set(r,b,g,1.0);
    val = ( GTS_VERTEX(v3)->p.z -zrange[0] )/range;    
    jet_color_map(val,r,g,b);
    (*gc._colors++).set(r,b,g,1.0);
  }
}
bool FileExists(string strFilename) {
  struct stat stFileInfo;
  bool blnReturn;
  int intStat;

  // Attempt to get the file attributes
  intStat = stat(strFilename.c_str(),&stFileInfo);
  if(intStat == 0) {
    // We were able to get the file attributes
    // so the file obviously exists.
    blnReturn = true;
  } else {
    // We were not able to get the file attributes.
    // This may mean that we don't have permission to
    // access the folder which contains this file. If you
    // need to do that level of checking, lookup the
    // return values of stat which will give you
    // more details on why stat failed.
    blnReturn = false;
  }
  
  return(blnReturn);
}
osg::ref_ptr<osg::Image>OSGExporter::cacheCompressedImage(IplImage *img,string name,int tex_size){
  string ddsname=osgDB::getNameLessExtension(name);
  ddsname +=".dds";
  IplImage *tex_img=cvCreateImage(cvSize(tex_size,tex_size),
				  IPL_DEPTH_8U,3);
  if(img && tex_img)
    cvResize(img,tex_img);
  else
    printf("Invalid Images\n");

  osg::ref_ptr<osg::Image> image= Convert_OpenCV_TO_OSG_IMAGE(tex_img);
  osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D;
  texture->setImage(image.get());
  compress(texture.get());
  osgDB::writeImageFile(*image.get(),ddsname);
  cvReleaseImage(&tex_img);
  return image;
}

osg::Image *OSGExporter::getCachedCompressedImage(string name,int size){
  IplImage *img=NULL;
  string basename=osgDB::getSimpleFileName(name);
  string ddsname=osgDB::getNameLessExtension(name);
  ddsname +=".dds";
  osg::Image *filecached=NULL;
  osg::Image *retImage=new osg::Image;
  if(compressed_img_cache.find(basename) == compressed_img_cache.end() || !compressed_img_cache[basename] ){
  
    if(FileExists(ddsname)){
      filecached=osgDB::readImageFile(ddsname);
    }else{
      img = cvLoadImage(name.c_str(),-1);
      if(!img){
	printf("In Cached Compressed Img Load failed %s.\n",name.c_str());
	return NULL;
      }
      else{
	filecached=cacheCompressedImage(img,ddsname,size).get();
      
      }
    }
     
    compressed_img_cache[basename]=filecached;

  }else{  
    filecached=compressed_img_cache[basename];
  }

  if(filecached->s() == size && filecached->t() == size)
    return filecached;

  int resize=filecached->s();
  unsigned int i=0;
  for(i=0; i < filecached->getNumMipmapLevels()-1; i++){
    if(resize <= size)
      break;
    resize >>= 1;
  }

  if(resize == 0)
    resize=1;

  int datasize=filecached->getTotalSizeInBytesIncludingMipmaps()-filecached->getMipmapOffset(i);
  unsigned char *newdata = new unsigned char[datasize];
  memcpy(newdata,filecached->getMipmapData(i),datasize);
  retImage->setImage(resize,resize,filecached->r(),filecached->getInternalTextureFormat() ,filecached->getPixelFormat(),filecached->getDataType(),newdata,osg::Image::USE_NEW_DELETE);
  osg::Image::MipmapDataType mipmaps;
  mipmaps=filecached->getMipmapLevels();
 
  int newsize= mipmaps[i]-mipmaps[i-1];
  int base=mipmaps[i];
 
  std::vector<unsigned int>newmipmap;



  for(int k=i; k < (int)mipmaps.size(); k++){
    newmipmap.push_back(newsize+(mipmaps[k]-base));
  }
 
  retImage->setMipmapLevels(newmipmap);

  
  
  return retImage;
}
#define TEXUNIT_ARRAY       0
#define TEXUNIT_HIST        2
#define TEXUNIT_INFO        3
#define TEXUNIT_PLANES       1

bool loadShaderSource(osg::Shader* obj, const std::string& fileName )
{
  std::string fqFileName = osgDB::findDataFile(fileName);
  if( fqFileName.length() == 0 )
    {
      std::cout << "File \"" << fileName << "\" not found." << std::endl;
      return false;
    }
  bool success = obj->loadShaderSourceFromFile( fqFileName.c_str());
  if ( !success  )
    {
      std::cout << "Couldn't load file: " << fileName << std::endl;
      return false;
    }
  else
    {
      return true;
    }
}

osg::TextureRectangle*  OSGExporter::getTextureHists(  CvHistogram *&finalhist){

   
   
   //long double total=0;
  
   float *tmpV= new float[16*16*16];
   osg::Image *dataImg= new osg::Image;
   dataImg->allocateImage(64,64,1,GL_LUMINANCE,GL_UNSIGNED_BYTE);
   //int count=0;
   float min=FLT_MAX;
   float max=FLT_MIN;
   float *ptr=tmpV;
   for(int i=0; i< 16; i++)
     for(int j=0; j< 16; j++)
       for(int k=0; k< 16; k++){
	 float val=cvQueryHistValue_3D(finalhist,i,j,k);


	 if(val == 0.0){
	   *ptr=FLT_MIN;
	   ptr++;
	   continue;
	 }
	 float logV=(-log(val));
	 if(logV< min)
	   min=logV;

	 if(logV> max)
	   max=logV;

	 *ptr=logV;
	 ptr++;
     
	 
	 /* cout << cvQueryHistValue_3D(finalhist,i,j,k) << " ";
	    total +=cvQueryHistValue_3D(finalhist,i,j,k);*/
       }
   printf("Min %f\n",min);
   printf("Max %f\n",max);
   float range=max-min;
   ptr=tmpV;
   for(int i=0;i < 64; i++)
     for(int j=0; j<64; j++){
       if(*ptr==FLT_MIN){
	 *ptr=0.0;
	 ptr++;
	 continue;
       }
      
       *ptr = (*ptr-min)*(255.0/range);
       //   printf("i: %d j: %d %f\n",i,j,*ptr); 
       ptr++;
     }
     cout << endl;//"\nTotal " << total <<endl;*/
  
     unsigned char *imgPtr=(unsigned char *)dataImg->data();
     ptr=tmpV;
     for(int i=0;i < 64; i++){
       for(int j=0; j<64; j++){
	 *imgPtr=(int)(*ptr);
   if(i ==1 && j==0 )
     *imgPtr=255;
	 //printf("i: %d j: %d %d\n",i,j,*imgPtr); 
	 ptr++;
	 imgPtr++;
       }
     }
     
  

   osg::TextureRectangle* texture = new osg::TextureRectangle(dataImg);
   return texture;

}

void OSGExporter::calcHists( MaterialToGeometryCollectionMap &mtgcm, map<int,string> textures, Hist_Calc &histCalc){

  MaterialToGeometryCollectionMap::iterator itr;
   for(itr=mtgcm.begin(); itr!=mtgcm.end(); ++itr){
     GeometryCollection& gc = itr->second;
     if(!gc._texturesActive )
       continue;
     string name=textures[itr->first];
      
     osg::Image *img=compressed_img_cache[name];
     if(!img){
       printf("Image null in getTextureHists %s\n",name.c_str());
       continue;
     }
     IplImage *tmp=cvCreateImage(cvSize(img->s(),img->t()),
				 IPL_DEPTH_8U,3);
     IplImage *mask=cvNewGray(tmp);
     cvZero(mask);

     DecompressImageRGB((unsigned char *)tmp->imageData,tmp->width,tmp->height,img->data(),squish::kDxt1);

     int count = 3;
     CvPoint pt[3];
     CvPoint* tri=pt;
     for(int i=0; i< (int) gc._texcoordArray->size(); i+=3){
       for(int j=0; j<3; j++){
	 
	 osg::Vec2f tc = (*gc._texcoordArray)[i+j];
	 tc *= tmp->width;
	 pt[j]=cvPoint((int)tc[1],(int)tc[0]);

       }
       
       cvFillPoly(mask, &tri, &count, 1, CV_RGB(255,255,255));
     }
    
     histCalc.calc_hist(tmp,mask,1/*BGR*/);
     if(gpuNovelty){
       osg::Texture *tex=osg_tex_map[itr->first];
       IplImage *rgba=cvCreateImage(cvSize(tmp->width,tmp->height),
				    IPL_DEPTH_8U,4);
       cvCvtColor(tmp, rgba, CV_RGB2RGBA);
       cvSetImageCOI(rgba,4);
       cvCopy(histCalc.imageTex,rgba);
       cvSetImageCOI(rgba,0);
       osg::Image *imageWithG=Convert_OpenCV_TO_OSG_IMAGE(rgba,false);
       osg::Texture2D* texture2D = dynamic_cast<osg::Texture2D*>(tex);
       texture2D->setImage(imageWithG);
       compress(texture2D,osg::Texture::USE_S3TC_DXT5_COMPRESSION);
     }
     cvReleaseImage(&tmp);
     cvReleaseImage(&mask);
   }
   
  
}

void OSGExporter::addNoveltyTextures( MaterialToGeometryCollectionMap &mtgcm, map<int,string> textures, Hist_Calc &histCalc,CvHistogram *hist){

  MaterialToGeometryCollectionMap::iterator itr;
   for(itr=mtgcm.begin(); itr!=mtgcm.end(); ++itr){
     GeometryCollection& gc = itr->second;
     if(!gc._texturesActive )
       continue;
     string name=textures[itr->first];
      
     osg::Image *img=compressed_img_cache[name];
     if(!img){
       printf("Image null in getTextureHists %s\n",name.c_str());
       continue;
     }
     IplImage *tmp=cvCreateImage(cvSize(img->s(),img->t()),
				 IPL_DEPTH_8U,3);
     IplImage *med=cvNewGray(tmp);

     DecompressImageRGB((unsigned char *)tmp->imageData,tmp->width,tmp->height,img->data(),squish::kDxt1);

 
     IplImage *novelty_image=  histCalc.get_novelty_img(tmp,hist); 
     //mcvNormalize((IplImage*)novelty_image, (IplImage*)novelty_image, 0, 255);
     IplImage *texInfo=cvNewColor(novelty_image);
     IplImage *tmpG=cvNewGray(novelty_image);
     mcvNormalize((IplImage*)novelty_image, (IplImage*)novelty_image, 0, 255);
     cvConvertScale(novelty_image,tmpG);
     
     cvSmooth(tmpG,med,CV_MEDIAN,9);
     //   scaleJetImage( (IplImage*)novelty_image, (IplImage*)testImg);
  
     cvSetImageCOI(texInfo,1);
     cvCopy(med,texInfo);
     cvSetImageCOI(texInfo,0);
     osg::Image *texI=Convert_OpenCV_TO_OSG_IMAGE(texInfo,false);
     osg::Texture2D* texture2D = new osg::Texture2D(texI);
     compress(texture2D,osg::Texture::USE_S3TC_DXT1_COMPRESSION);
     gc._geom->getStateSet()->setTextureAttribute(TEXUNIT_INFO,texture2D );
     cvReleaseImage(&texInfo);
     cvReleaseImage(&tmpG);
     cvReleaseImage(&novelty_image);
     cvReleaseImage(&med);
     cvReleaseImage(&tmp);

   }
   
  
}


bool OSGExporter::convertGtsSurfListToGeometry(GtsSurface *s, map<int,string> textures,ClippingMap *cm,int tex_size,osg::ref_ptr<osg::Geode>*group,vector<Plane3D> planes,vector<TriMesh::BBox> bounds,VerboseMeshFunc vmcallback,float *zrange)
{
   _tex_size=tex_size;
  MaterialToGeometryCollectionMap mtgcm;
  gpointer data[9];
  gint n=0;
  data[0]=&mtgcm;
  data[1] = &n;
  data[2]=cm;
  data[3]=&textures;
  data[4]=zrange;
  data[6]=zrange;
  data[7]=(void *)_planeTexSize;
  data[8]=(void *)usePlaneDist;
  //data[5]=hists;
  //tempFF=getPlaneTex(planes);
  gts_surface_foreach_face (s, (GtsFunc) bin_face_mat_osg , data);
  MaterialToGeometryCollectionMap::iterator itr;
  int tex_count=0;
  for(itr=mtgcm.begin(); itr!=mtgcm.end(); ++itr){
    GeometryCollection& gc = itr->second;
    if (gc._numPrimitives){
      
       
      gc._geom = new osg::Geometry;
       
      osg::Vec3Array* vertArray = new osg::Vec3Array(gc._numPoints);
      gc._vertices = vertArray->begin();
      gc._geom->setVertexArray(vertArray);
       
      // set up color.
      {
	osg::Vec4Array* colorsArray = new osg::Vec4Array(gc._numPoints);
	 
	 
	gc._colors=colorsArray->begin();                 
	gc._colorsActive=true;
	gc._geom->setColorArray(colorsArray);
	 
	gc._geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
      }

      // set up texture if needed.
   
      if (itr->first >= 0 &&  textures.count(itr->first) > 0){
	 
	std::string filename=prefixdir+textures[itr->first];
	osg::notify(osg::INFO) << "ctex " << filename  << std::endl;
	char fname[255];
	sprintf(fname,"mesh/%s",textures[itr->first].c_str());

	if(vmcallback)
	  vmcallback(++tex_count,mtgcm.size());
	if(verbose)
	  printf("\rLoading Texture: %03d/%03d",++tex_count,mtgcm.size());
	if(!ive_out)
	  if(verbose)printf("\n");	 
	fflush(stdout); 
	
	 
	osg::ref_ptr<osg::Image> image=getCachedCompressedImage(filename,tex_size);

	  //LoadResizeSave(filename,fname, (!ive_out),tex_size);
	if (image.valid()){	     
	// create state
	  osg::StateSet* stateset = new osg::StateSet;
	  // create texture
	  osg::Texture2D* texture = new osg::Texture2D;
	  // texture->setUnRefImageDataAfterApply( false );
	  texture->setImage(image.get());
	  //texture->setInternalFormatMode(internalFormatMode);
	  stateset->setTextureAttributeAndModes(0,texture,
						osg::StateAttribute::ON);
	  if(usePlaneDist){
	    stateset->setMode(GL_BLEND,osg::StateAttribute::OFF);
	    stateset->setMode(GL_ALPHA_TEST,osg::StateAttribute::OFF);
	   
	  }else{

	 
	    osg::TexEnvCombine *te = new osg::TexEnvCombine;    
	    // Modulate diffuse texture with vertex color.
	    te->setCombine_RGB(osg::TexEnvCombine::REPLACE);
	    te->setSource0_RGB(osg::TexEnvCombine::TEXTURE);
	    te->setOperand0_RGB(osg::TexEnvCombine::SRC_COLOR);
	    te->setSource1_RGB(osg::TexEnvCombine::PREVIOUS);
	    te->setOperand1_RGB(osg::TexEnvCombine::SRC_COLOR);
	    
	    // Alpha doesn't matter.
	    te->setCombine_Alpha(osg::TexEnvCombine::REPLACE);
	    te->setSource0_Alpha(osg::TexEnvCombine::PREVIOUS);
	    te->setOperand0_Alpha(osg::TexEnvCombine::SRC_ALPHA);
	    
	    stateset->setTextureAttribute(0, te);
	  }
	  gc._texturesActive=true;
	  stateset->setDataVariance(osg::Object::STATIC);
	   
	  gc._geom->setStateSet(stateset);
	     
	  osg::Vec2Array* texcoordArray = new osg::Vec2Array(gc._numPoints);
	  gc._texcoords = texcoordArray->begin();
	  gc._texcoordArray = texcoordArray;
	  gc._geom->setTexCoordArray(0,texcoordArray);
	  if(usePlaneDist){
	    osg::Vec2Array* texcoordPlanes = new osg::Vec2Array(gc._numPoints);
	    gc._texcoordsPlane = texcoordPlanes->begin();
	    gc._geom->setTexCoordArray(TEXUNIT_PLANES,texcoordPlanes);
	    gc._planeTexValid=true;
	  }
	  //	  if(compress_tex && _hardware_compress && !do_atlas)
	  //compress2(texture);
	  
	  	 
	  osg_tex_ptrs.push_back(texture);
	  osg_tex_map[itr->first]=texture;
	  }
      }
    }
  }
  if(verbose)
    printf("\n");
 
  
    gts_surface_foreach_face (s, (GtsFunc) add_face_mat_osg , data);
    osg::ref_ptr<osg::Geode> untextured = new osg::Geode;
    osg::ref_ptr<osg::Geode> textured = new osg::Geode;
    osg::TextureRectangle* histTex=NULL;
    osg::Program* program=NULL;

   
     
      program = new osg::Program;
      program->setName( "microshader" );
      osg::Shader *novelty=new osg::Shader( osg::Shader::FRAGMENT);
      osg::Shader *distNovelty=new osg::Shader( osg::Shader::VERTEX);
      if(gpuNovelty){
	loadShaderSource( novelty, basedir+"novelty-live.frag" );
      }else{
	loadShaderSource( novelty, basedir+"novelty.frag" );
	//	loadShaderSource( novelty, "ass.frag" );
      }
      loadShaderSource( distNovelty, basedir+"novelty.vert" );
      //loadShaderSource( distNovelty, "ass.vert" );
      
      program->addShader(  novelty );
       program->addShader( distNovelty );
 
      if(computeHists){
	
	Hist_Calc histCalc(_tex_size,_tex_size,16,1);
	CvHistogram *finalhist=NULL;    
	calcHists(mtgcm,textures,histCalc);
	histCalc.get_hist(finalhist);
	if(gpuNovelty)
	  histTex= getTextureHists(finalhist);
	else
	  addNoveltyTextures(mtgcm,textures,histCalc,finalhist);
      }
      
      
      // add everthing into the Geode.   
      osg::TextureRectangle* planeTex=	 getPlaneTex(planes,_planeTexSize);
      
      
      osgUtil::SmoothingVisitor smoother;
      for(itr=mtgcm.begin();
      itr!=mtgcm.end();
      ++itr)
    {
      GeometryCollection& gc = itr->second;
      if (gc._geom)
        {
	  gc._geom->setUseDisplayList(false);
          //  tessellator.retessellatePolygons(*gc._geom);
        
	  smoother.smooth(*gc._geom);
	    if(gc._texturesActive){
	 
	      if(gpuNovelty){
	      gc._geom->getStateSet()->addUniform(new osg::Uniform("hist", 
								   TEXUNIT_HIST) );
	      gc._geom->getStateSet()->setTextureAttribute(TEXUNIT_HIST,
							   histTex);
	      gc._geom->getStateSet()->addUniform( new osg::Uniform("binsize",
								    16.0f));
	    }
	    if(usePlaneDist){
		gc._geom->getStateSet()->addUniform( new osg::Uniform( "shaderOut", 1));
		gc._geom->getStateSet()->addUniform( new osg::Uniform( "weights", osg::Vec3(0.025f, 0.50f, 0.4f) ));
		
		
		gc._geom->getStateSet()->addUniform(new osg::Uniform("rtex",0));
		gc._geom->getStateSet()->addUniform(new osg::Uniform("infoT",
								     TEXUNIT_INFO));
		
		gc._geom->getStateSet()->addUniform(new osg::Uniform("planes", 
								     TEXUNIT_PLANES) );
		gc._geom->getStateSet()->setTextureAttribute(TEXUNIT_PLANES,
							     planeTex);
		gc._geom->getStateSet()->setAttributeAndModes( program,
							       osg::StateAttribute::ON );
	      }
	      
	      textured->addDrawable(gc._geom);
    }
	 
	  
	  else{
	    untextured->addDrawable(gc._geom);
	  }

        }

    }
      if(usePlaneDist){
	osg::StateSet* _stateset = new osg::StateSet;
	osg::PolygonMode* _polygonmode = new osg::PolygonMode;
	_polygonmode->setMode(osg::PolygonMode::FRONT_AND_BACK,
			      osg::PolygonMode::LINE);
	_stateset->setAttribute(_polygonmode);
	_stateset->setMode( GL_LIGHTING, osg::StateAttribute::PROTECTED | osg::StateAttribute::OFF );
   
	
	
	
	for(int i=0; i<(int)bounds.size(); i++){
	  osg::ref_ptr<osg::Box> b=new osg::Box(osg::Vec3(bounds[i].center()[1],
							  bounds[i].center()[0],
							  -bounds[i].center()[2]),
						bounds[i].size()[1],
						bounds[i].size()[0],
						bounds[i].size()[2]);
	  
	  
	  osg::ShapeDrawable* _shapedrawable = new osg::ShapeDrawable;
	  _shapedrawable->setColor(osg::Vec4(1.0,0.0,0.0,0.80));
	  _shapedrawable->setShape(b.get());
	  _shapedrawable->setStateSet(_stateset); 
	  textured->addDrawable(_shapedrawable);
	  osg::Geometry* linesGeom = new osg::Geometry();
	  Plane3D p=planes[i];
	  float tmp = p.u[0];
	  p.u[0]=p.u[1];
	  p.u[1]=tmp;
	  p.u[2]=-p.u[2];
	  
	  
	  osg::Vec3Array* vertices = displayPlane(p,Point3D(bounds[i].center()[1],bounds[i].center()[0],-bounds[i].center()[2]));
	  
	  // pass the created vertex array to the points geometry object.
	  linesGeom->setVertexArray(vertices);
	  
	  // set the colors as before, plus using the above
	  osg::Vec4Array* colors = new osg::Vec4Array;
	  colors->push_back(osg::Vec4(0.0f,1.0f,0.0f,1.0f));
	  linesGeom->setColorArray(colors);
	  linesGeom->setColorBinding(osg::Geometry::BIND_OVERALL);
	  
	  
	  // set the normal in the same way color.
	  osg::Vec3Array* normals = new osg::Vec3Array;
	  normals->push_back(osg::Vec3(0.0f,-1.0f,0.0f));
	  linesGeom->setNormalArray(normals);
	  linesGeom->setNormalBinding(osg::Geometry::BIND_OVERALL);
	  
	  
	  // This time we simply use primitive, and hardwire the number of coords to use 
    // since we know up front,
	  linesGeom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES,0,vertices->size()));
	  linesGeom->setStateSet(_stateset); 
	  textured->addDrawable(linesGeom); 
	}
      

  
	osg::Point* point = new osg::Point();
	point->setSize( 4.0 );
	textured->getOrCreateStateSet()->setAttribute( point, osg::StateAttribute::ON );
      }

 if(textured->getNumDrawables())
  group[0]=textured.get();
 else 
   group[0]=NULL;

 if(untextured->getNumDrawables() )
   group[1]=untextured.get();
 else
   group[1]=NULL;

  return true;
}

bool OSGExporter::convertGtsSurfListToGeometryTexArray(GtsSurface *s, map<int,string> textures,ClippingMap *cm,int tex_size,osg::ref_ptr<osg::Geode>*group,VerboseMeshFunc vmcallback,float *zrange)
{
   _tex_size=tex_size;
  GeometryCollection gc;
  gpointer data[6];
  map<int,int> texnum2arraynum;
  gint n=0;
  data[0]=&gc;
  data[1] = &n;
  data[2]=cm;
  data[3]=&textures;
  data[4]=zrange;
  data[5]=&texnum2arraynum;
  gts_surface_foreach_face (s, (GtsFunc) bin_face_all_osg , data);
  
  osg::ref_ptr<osg::Geode> untextured = new osg::Geode;
  osg::ref_ptr<osg::Geode> textured = new osg::Geode;

  if (gc._numPrimitives){
      
       
    gc._geom = new osg::Geometry;
       
    osg::Vec3Array* vertArray = new osg::Vec3Array(gc._numPoints);
    gc._vertices = vertArray->begin();
    gc._geom->setVertexArray(vertArray);
    
    // set up color.
    {
      osg::Vec4Array* colorsArray = new osg::Vec4Array(gc._numPoints);
	
      
      gc._colors=colorsArray->begin();                 
      gc._colorsActive=true;
      gc._geom->setColorArray(colorsArray);
      
      gc._geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
    }
    


    osg::Program* program=NULL;
    int num_valid_tex=0;
    int tex_count=0;
    map<int,string>::iterator itr;
    osg::ref_ptr<osg::Texture2DArray> textureArray= new osg::Texture2DArray; 
    program = new osg::Program;
    program->setName( "microshader" );
    osg::Shader *lerp=new osg::Shader( osg::Shader::FRAGMENT);
    loadShaderSource( lerp, basedir+"lerp.frag" );
    program->addShader(  lerp );
  
  
    for(itr=textures.begin(); itr!=textures.end(); ++itr){
      if (itr->first >= 0){
	texnum2arraynum[itr->first]=	num_valid_tex++;
      }
    }  
  
    textureArray->setTextureSize(tex_size,tex_size,num_valid_tex);
    
    int imgNum=0;

    osg::StateSet* stateset = new osg::StateSet;
    stateset->setAttributeAndModes( program, osg::StateAttribute::ON );
    
  
    stateset->addUniform( new osg::Uniform("theTexture", TEXUNIT_ARRAY) );
    stateset->addUniform( new osg::Uniform( "weights", osg::Vec3(0.025f, 0.10f, 0.4f) ));
    stateset->addUniform( new osg::Uniform( "shaderOut", 1));

    stateset->setTextureAttribute(TEXUNIT_ARRAY, textureArray.get());
  
  
    for(itr=textures.begin(); itr!=textures.end(); ++itr){
    
      std::string filename=prefixdir+itr->second;
      osg::notify(osg::INFO) << "ctex " << filename  << std::endl;
      char fname[255];
      sprintf(fname,"mesh/%s",itr->second.c_str());

      if(vmcallback)
	vmcallback(++tex_count,textures.size());
      if(verbose)
	printf("\rLoading Texture: %03d/%03d",++tex_count,textures.size());
      if(!ive_out)
	if(verbose)printf("\n");	 
      fflush(stdout); 
	
	 
      osg::ref_ptr<osg::Image> image=getCachedCompressedImage(filename,tex_size);
    
      //LoadResizeSave(filename,fname, (!ive_out),tex_size);
      if (image.valid()){	     
	 
	// create state
	
	  
	textureArray->setImage(imgNum,image.get());
	textureArray->setUnRefImageDataAfterApply(false);
	gc._texturesActive=true;
	stateset->setDataVariance(osg::Object::STATIC);
	   
	gc._geom->setStateSet(stateset);
	     
	osg::Vec2Array* texcoordArray = new osg::Vec2Array(gc._numPoints);
	gc._texcoords = texcoordArray->begin();
	gc._geom->setTexCoordArray(0,texcoordArray);
	for(int i=0; i< NUM_TEX_BLEND_COORDS; i++){
	  osg::Vec3Array* texcoordBlendArray =new osg::Vec3Array(gc._numPoints);
	  gc._texcoordsTexArray[i] = texcoordBlendArray->begin();
	  gc._geom->setTexCoordArray(i+1,texcoordBlendArray);
	}
	osg_tex_arr_ptrs.push_back(textureArray);
	imgNum++;
      }
    }
  
  
  
    if(verbose)
      printf("\n");
  
    gts_surface_foreach_face (s, (GtsFunc) add_face_all_osg , data);
  
   
  
    // osgUtil::Tessellator tessellator;
    
    // add everthing into the Geode.   
  
    osgUtil::SmoothingVisitor smoother;
   
    if (gc._geom){
    
      //  tessellator.retessellatePolygons(*gc._geom);
    
      smoother.smooth(*gc._geom);
      if(gc._texturesActive)
	textured->addDrawable(gc._geom);
      else{
	untextured->addDrawable(gc._geom);
      }
      //  printf("Grilled Shrip %f %f %f %f\n",gc._texLimits.xMin(),
      //   gc._texLimits.xMax(),gc._texLimits.yMin(),gc._texLimits.yMax());
    }
  
  }
  
 
 
  if(textured->getNumDrawables())
    group[0]=textured.get();
  else 
    group[0]=NULL;

  if(untextured->getNumDrawables() )
    group[1]=untextured.get();
  else
    group[1]=NULL;

  return true;
}

bool OSGExporter::outputModelOSG(char *out_name,  osg::ref_ptr<osg::Geode> *group) {

  osg::Geode *tex=group[0].get();
  osg::Geode *untex =group[1].get();
 
  osgDB::ReaderWriter::WriteResult result;
  char outtex[255];
  string outname_str(out_name);
  if(tex && tex->getNumDrawables()){
    strcpy(outtex,(outname_str.substr(0,outname_str.length()-4)+string("-t.ive")).c_str());
    
    result = osgDB::Registry::instance()->writeNode(*tex,outtex,osgDB::Registry::instance()->getOptions());
    if (result.success())	{
      if(verbose)
	osg::notify(osg::NOTICE)<<"Data written to '"<<outtex<<"'."<< std::endl;
    }
    else if  (result.message().empty()){
      osg::notify(osg::NOTICE)<<"Warning: file write to '"<<outtex<<"' no supported."<< std::endl;
      // root->getChild(0)=NULL;
    }else
      osg::notify(osg::NOTICE)<<"Writer output: "<< result.message()<<std::endl;
  }  

  if(untex && untex->getNumDrawables()){
    strcpy(outtex,(outname_str.substr(0,outname_str.length()-4)+string("-u.ive")).c_str());
    
    
    result = osgDB::Registry::instance()->writeNode(*untex,outtex,osgDB::Registry::instance()->getOptions());
    if (result.success())	{
      if(verbose)
	osg::notify(osg::NOTICE)<<"Data written to '"<<outtex<<"'."<< std::endl;
    }
    else if  (result.message().empty()){
      osg::notify(osg::NOTICE)<<"Warning: file write to '"<<outtex<<"' no supported."<< std::endl;
      // root->getChild(0)=NULL;
    }
  }
  
  return true;

  /*  root->removeChild(0,1);
      geode=NULL;*/
  
}

std::string relative_path(std::string pathString){
  std::string a = pathString;
  unsigned int f = pathString.rfind('/');
  if (f < pathString.length()) {
    a = a.substr(f+1);
  }
  return (a);
}




osg::Image *OSGExporter::LoadResizeSave(string filename,string outname,bool save,int tex_size){

  IplImage *fullimg=NULL;
  bool cached=false;
  if(tex_image_cache.find(filename) == tex_image_cache.end() || !tex_image_cache[filename] ){
    fullimg=cvLoadImage(filename.c_str(),-1);
   
  }
  else{

    fullimg=tex_image_cache[filename];
   
    if(fullimg && fullimg->width < tex_size){
      printf("Warning Upsampling from cached so reloading\n");
      fullimg=cvLoadImage(filename.c_str(),-1);
    }else{
      cached=true;
    }
  }

  IplImage *cvImg=cvCreateImage(cvSize(tex_size,tex_size),
				IPL_DEPTH_8U,3);
  
  if(fullimg){
    
    cvResize(fullimg,cvImg);
    cvReleaseImage(&fullimg);
    tex_image_cache[filename]=cvImg;
  }
  
  else {
    printf("Failed to load %s\n",filename.c_str());
    cvReleaseImage(&cvImg);
    return NULL;
  }

  osg::Image* image=NULL;
  
  if(compress_tex && !_hardware_compress){
    image =Convert_OpenCV_TO_OSG_IMAGE(cvImg,!cached,true);
  }else
    image =Convert_OpenCV_TO_OSG_IMAGE(cvImg,!cached,false);
 
  image->setFileName(osgDB::getSimpleFileName(filename));
  if(save){  
    //printf("Writing %s\n",outname.c_str());
    osgDB::writeImageFile(*image,outname);
    image->setFileName(outname);
  }
  return image;
}

enum {TEX_VALID,TEX_MARGIN,TEX_NOT_VALID};
static int find_min_project_dist_bbox(GtsVertex ** triVert,GtsMatrix* back_trans,Camera_Calib *calib, double &dist){

  double vX[3],vY[3];
  GtsPoint tmpP[3];

  
  for(int i=0; i< 3; i++)
    tmpP[i]=triVert[i]->p;
 
  for(int i=0; i < 3; i++){
    gts_point_transform(&tmpP[i],back_trans);
    camera_frame_to_dist_pixel_coords(*calib,tmpP[i].x,tmpP[i].y,tmpP[i].z,
				      vX[i],vY[i]);
  }
  int ret=TEX_VALID;
 
  for(int i=0; i < 3; i++){
    if(vX[i]> calib->width || vY[i] > calib->height || vX[i] < 0.0 || vY[i] < 0.0  ){ ret=TEX_NOT_VALID;

    }
  }
  if(ret==TEX_NOT_VALID){
    int testMargin=1;
    for(int i=0; i < 3; i++){
      if(vX[i]> calib->width+texMargin || vY[i] > calib->height+texMargin || vX[i] < 0.0 -texMargin || vY[i] < 0.0 -texMargin ){
	testMargin=0;
      }
    }
    if(testMargin)
      ret=TEX_MARGIN;
  }

  dist=0.0;
  for(int i=0; i < 3; i++){
    dist+= sqrt(pow((calib->width/2.0)-vX[i],2)+pow((calib->height/2.0)-vY[i],2));
    //((  (vY[i])-(calib->width/2.0))+ (vX[i]-(calib->height/2.0))/2.0) ;
   
    // 1/min(pow(vX[i]-calib->width-(calib->width/2.0),2),
    //	pow(vY[i]-calib->height-(calib->height/2.0),2));
  
  }


  // sqrt(pow((calib->width/2.0)-vX[i],2)+pow((calib->height/2.0)-vY[i],2)); 
  /*  printf("%f(%f) %f(%f) = %f\n",vY[0],
      (  (vY[0])-(calib->width/2.0)),
      vX[0],
      (vX[0]-(calib->height/2.0)),
	   
	   
      );*/
  //  100000/min(pow(vX[0]-calib->width-(calib->width/2.0),2),
  //pow(vY[0]-calib->height-(calib->height/2.0),2));
  //pow(vY[i]-calib->height-(calib->height/2.0),2))
  /*1/max(pow(vX-calib->width-(calib->width/2.0),2),
    pow(vY-calib->height-(calib->height/2.0),2));*/
  //
  return ret;
}

static void set_tex_id_unknown (TVertex *v)
{
  v->id= -1;
}

gdouble dist_to_bbox_center(GtsBBox*bb,GtsPoint *pt){
  GtsPoint center;
  center.x = (bb->x1 + bb->x2) / 2.0f;
  center.y = (bb->y1 + bb->y2) / 2.0f;
  center.z = (bb->z1 + bb->z2) / 2.0f;
  return gts_point_distance(pt,&center);
}
int find_closet_img_trans(GtsTriangle *t,GNode* bboxTree, std::vector<GtsPoint> camPosePts,std::map<int,GtsMatrix *> back_trans,Camera_Calib *calib,int type){
 
  
  if(bboxTree==NULL)
    return 0;
  int index=INT_MAX;
  if(type ==0){
    /*   GtsVertex * v1,* v2,* v3;
	 gts_triangle_vertices(t,(GtsVertex **)& v1, 
	 (GtsVertex **)&v2, (GtsVertex **)&v3);
    
	 fprintf(errFP,"%f %f %f %f %f %f %f %f %f \n",
	 v1->p.x,v1->p.y,v1->p.z,v2->p.x,v2->p.y,v2->p.z,
	 v3->p.x,v3->p.y,v3->p.z);
	
	 GtsPoint tc;
	 tc.x=(v1->p.x+v2->p.x+v3->p.x)/3.0;
	 tc.y=(v1->p.y+v2->p.y+v3->p.y)/3.0;
	 tc.z=(v1->p.z+v2->p.z+v3->p.z)/3.0;
	 int bestProjection=INT_MAX;
	 int p=lastBP;
	 for(p -=2; p < lastBP+2; p++){
	 GtsPoint tcT=tc;
	 gts_point_transform(&tcT,back_trans[p]);
	 double vX,vY;
	 tcT=v1->p;
	 gts_point_transform(&tcT,back_trans[p]);
	 camera_frame_to_dist_pixel_coords(*calib,tcT.x,tcT.y,tcT.z,vX,vY);
	 fprintf(errFP,"V1 proj:%d %f %f %f %f %f\n",p,tcT.x,tcT.y,tcT.z,vX,vY);
	 tcT=v2->p;
	 gts_point_transform(&tcT,back_trans[p]);
	 camera_frame_to_dist_pixel_coords(*calib,tcT.x,tcT.y,tcT.z,vX,vY);
	 fprintf(errFP,"V2 %d %f %f %f %f %f\n",p,tcT.x,tcT.y,tcT.z,vX,vY);
	 tcT=v3->p;
	 gts_point_transform(&tcT,back_trans[p]);
	 camera_frame_to_dist_pixel_coords(*calib,tcT.x,tcT.y,tcT.z,vX,vY);
	 fprintf(errFP,"V3 %d %f %f %f %f %f\n",p,tcT.x,tcT.y,tcT.z,vX,vY);
	 }
	 GSList * list ,*itr;
    
    
	 GtsVertex * v[3];
	 gts_triangle_vertices(t,(GtsVertex **)& v[0], 
	 (GtsVertex **)&v[1], (GtsVertex **)&v[2]);
    
	 tc.x=(v[0]->p.x+v[1]->p.x+v[2]->p.x)/3.0;
	 tc.y=(v[0]->p.y+v[1]->p.y+v[2]->p.y)/3.0;
	 tc.z=(v[0]->p.z+v[1]->p.z+v[2]->p.z)/3.0;
   
	 bestProjection=INT_MAX;
	 int quickMethod=1;

	 itr = list =  gts_bb_tree_point_closest_bboxes(bboxTree,&tc);
	 double minDist=DBL_MAX;
	 double dist=DBL_MAX;
	 while (itr) {
	 GtsBBox * bbox=GTS_BBOX (itr->data);
	 if(!gts_bbox_point_is_inside(bbox,&tc)){
	 itr = itr->next;
	 continue;
	 }     
	 int val= (int)bbox->bounded;
	 if(find_min_project_dist_bbox(v,back_trans[val],calib, dist))
	 fprintf(errFP,"Aleged valid %d\n",val);
	
	 itr = itr->next;
	 }
	 fflush(errFP);
	 return INT_MAX;*/
    /*
      for(int i=0; i < (int)camPosePts.size(); i++){
      double dist=gts_point_triangle_distance(&camPosePts[i],t);
      
      GtsVertex * v1,* v2,* v3;
      gts_triangle_vertices(t,(GtsVertex **)& v1, 
      (GtsVertex **)&v2, (GtsVertex **)&v3);
      printf("%f %f %f %f %f %f dist: %f\n",//transP.x,transP.y,transP.z,
      v1->p.x,v1->p.y,v1->p.z, dist);
      
      if(dist < minDist){
      minDist=dist;
      index=i;
      }
      }*/
  }else{
    GSList * list ,*itr;
    
    
    GtsVertex * v[3];
    gts_triangle_vertices(t,(GtsVertex **)& v[0], 
			  (GtsVertex **)&v[1], (GtsVertex **)&v[2]);
    GtsPoint tc;
    tc.x=(v[0]->p.x+v[1]->p.x+v[2]->p.x)/3.0;
    tc.y=(v[0]->p.y+v[1]->p.y+v[2]->p.y)/3.0;
    tc.z=(v[0]->p.z+v[1]->p.z+v[2]->p.z)/3.0;
   
    int bestProjection=INT_MAX;

    itr = list =  gts_bb_tree_point_closest_bboxes(bboxTree,&tc);
    double minDist=DBL_MAX;
    double dist=DBL_MAX;
    
    while (itr) {
      GtsBBox * bbox=GTS_BBOX (itr->data);
      if(!gts_bbox_point_is_inside(bbox,&tc)){
	itr = itr->next;
	continue;
      }     
      int val= (int)bbox->bounded;
      if(find_min_project_dist_bbox(v,back_trans[val],calib, dist) == TEX_VALID){
	if(dist < minDist){
	  minDist=dist;
	  bestProjection=val;
	}
      }
      itr = itr->next;
    }
    g_slist_free (list);   
    
    //Brute force quick method failed
    if(bestProjection == INT_MAX){
      minDist=DBL_MAX;
      for(int i =0; i < (int)bboxes_all.size(); i++){
	if(gts_bbox_point_is_inside(bboxes_all[i],&tc)){
	  int val= (int)bboxes_all[i]->bounded;
	  if(find_min_project_dist_bbox(v,back_trans[val],calib, dist) == TEX_MARGIN){
	    if(dist < minDist){
	      minDist=dist;
	      bestProjection=val;
	    }
	  }
	}
      }
    }
    
    return bestProjection;
  }
  
  return index;

}

bool find_blend_img_trans(GtsTriangle *t,GNode* bboxTree, std::vector<GtsPoint> camPosePts,std::map<int,GtsMatrix *> back_trans,Camera_Calib *calib,int *idx){
  
  
  if(bboxTree==NULL)
    return 0;
 

  for(int i=0; i < NUM_TEX_BLEND_COORDS; i++)
    idx[i]=INT_MAX;
  
  GSList * list ,*itr;
  GtsVertex * v[3];
  
  gts_triangle_vertices(t,(GtsVertex **)& v[0], 
			(GtsVertex **)&v[1], (GtsVertex **)&v[2]);
  GtsPoint tc;
  tc.x=(v[0]->p.x+v[1]->p.x+v[2]->p.x)/3.0;
  tc.y=(v[0]->p.y+v[1]->p.y+v[2]->p.y)/3.0;
  tc.z=(v[0]->p.z+v[1]->p.z+v[2]->p.z)/3.0;
  
  int bestProjection=INT_MAX;
  
  itr = list =  gts_bb_tree_point_closest_bboxes(bboxTree,&tc);
  double minDist=DBL_MAX;
  double dist=DBL_MAX;
  std::vector<pair<gdouble,int> > dists;

  while (itr) {
    GtsBBox * bbox=GTS_BBOX (itr->data);
    if(!gts_bbox_point_is_inside(bbox,&tc)){
      itr = itr->next;
      continue;
    }     
    int val= (int)bbox->bounded;
    if(find_min_project_dist_bbox(v,back_trans[val],calib, dist) == TEX_VALID){
      dists.push_back(make_pair(dist,val));
    }
  
    itr = itr->next;
  }
  g_slist_free (list);   
  if(dists.size()){
    sort( dists.begin(), dists.end() );
    for(int i=0; i< NUM_TEX_BLEND_COORDS; i++)
      idx[i]=dists[i].second;
   
    return true;

  }else{

      minDist=DBL_MAX;
      for(int i =0; i < (int)bboxes_all.size(); i++){
	if(gts_bbox_point_is_inside(bboxes_all[i],&tc)){
	  int val= (int)bboxes_all[i]->bounded;
	  if(find_min_project_dist_bbox(v,back_trans[val],calib, dist) == TEX_MARGIN){
	    if(dist < minDist){
	      minDist=dist;
	      bestProjection=val;
	    }
	  }
	}
      }
      for(int i=0; i < NUM_TEX_BLEND_COORDS; i++)
	idx[i]=bestProjection;
  }
  
  return true;
}

typedef struct _texGenData{
  GNode *bboxTree;
  std::vector<GtsPoint> camPosePts;
  int verbose;
  
  int tex_size;
  std::map<int,GtsMatrix *> back_trans;
  int validCount;
  int count;
  int reject;
  int total;
  std::vector<T_Face *> *border_faces;
  Camera_Calib *calib;
}texGenData;


static void texcoord_foreach_face (T_Face * f,
				   texGenData *data)
{
  
  int indexClosest=find_closet_img_trans(&GTS_FACE(f)->triangle,
					 data->bboxTree,data->camPosePts,
					 data->back_trans,data->calib,1);
  fprintf(ffp,"%d\n",indexClosest);
  if(indexClosest == INT_MAX){
    /*fprintf(errFP,"Failed traingle\n");
      gts_write_triangle(&GTS_FACE(f)->triangle,NULL,errFP);
      fflush(errFP);*/
    if(data->verbose)
      libpolyp::tex_add_verbose(data->count++,data->total,data->reject++);
    return;
  }
    
  if(apply_tex_to_tri(f,data->calib,data->back_trans[indexClosest],indexClosest,data->tex_size,texMargin))
    data->validCount++;
  else{
    printf("Index closest %d\n",indexClosest);
    find_closet_img_trans(&GTS_FACE(f)->triangle,
			  data->bboxTree,data->camPosePts,
			  data->back_trans,data->calib,0);
  }
 
  if(data->verbose)
    tex_add_verbose(data->count++,data->total,data->reject);

}

static void texcoord_blend_foreach_face (T_Face * f,
				   texGenData *data)
{
  int idx[NUM_TEX_BLEND_COORDS];
  bool found=find_blend_img_trans(&GTS_FACE(f)->triangle,
					 data->bboxTree,data->camPosePts,
					 data->back_trans,data->calib,idx);
  for(int i=0; i <NUM_TEX_BLEND_COORDS; i++)
  fprintf(ffp,"%d ",idx[i]);
  fprintf(ffp,"\n");
  if(!found){
    /*fprintf(errFP,"Failed traingle\n");
      gts_write_triangle(&GTS_FACE(f)->triangle,NULL,errFP);
      fflush(errFP);*/
    if(data->verbose)
      libpolyp::tex_add_verbose(data->count++,data->total,data->reject++);
    return;
  }
    
  if(apply_blend_tex_to_tri(f,data->calib,data->back_trans,idx,data->tex_size,texMargin))
    data->validCount++;
  else{
    //printf("Failed\n");
  }
 
  if(data->verbose)
    tex_add_verbose(data->count++,data->total,data->reject);

}
static void findborder_blend_foreach_face (T_Face * f,
				     texGenData *data)
{
  GtsTriangle * t = &GTS_FACE(f)->triangle;
  TVertex * v1,* v2,* v3; 
  gts_triangle_vertices(t,(GtsVertex **)& v1, 
			(GtsVertex **)&v2, (GtsVertex **)&v3);
  for(int i=0; i < NUM_TEX_BLEND_COORDS; i++){
    if(f->materialB[i] != v1->idB[i] || f->materialB[i] != v2->idB[i] || f->materialB[i] != v3->idB[i]){
    boost::mutex::scoped_lock scoped_lock(bfMutex);
    data->border_faces->push_back(f);
    return;
    }
  }
}


static void findborder_foreach_face (T_Face * f,
				     texGenData *data)
{
  GtsTriangle * t = &GTS_FACE(f)->triangle;
  TVertex * v1,* v2,* v3; 
  gts_triangle_vertices(t,(GtsVertex **)& v1, 
			(GtsVertex **)&v2, (GtsVertex **)&v3);
  if(f->material != v1->id || f->material != v2->id || f->material != v3->id){
    boost::mutex::scoped_lock scoped_lock(bfMutex);
    data->border_faces->push_back(f);
  }
}

void gen_mesh_tex_coord(GtsSurface *s ,Camera_Calib *calib, std::map<int,GtsMatrix *> back_trans,GNode * bboxTree,int tex_size, int num_threads,int verbose,int blend){
    ffp=fopen("w.txt","w");
  
  //errFP=fopen("err.txt","w");
  std::vector<GtsPoint> camPosePts;
  GtsPoint transP;
  if(verbose)
    printf("Size %d\n",back_trans.size());
  map<int,GtsMatrix *>::iterator iter;
  for(  iter=back_trans.begin();  iter != back_trans.end(); iter++){
  
    GtsMatrix *m= gts_matrix_inverse(iter->second); 
    transP.x=m[0][3];
    transP.y=m[1][3];
    transP.z=m[2][3];
    camPosePts.push_back(transP);
    gts_matrix_destroy(m);
  }

  
  gts_surface_foreach_vertex(s,(GtsFunc)set_tex_id_unknown,NULL);

  std::vector<T_Face *> border_faces;

  texGenData tex_data;
  tex_data.bboxTree=bboxTree;
  tex_data.camPosePts =camPosePts;
  
  
  tex_data.back_trans=back_trans;
  tex_data.validCount=0;
  tex_data.tex_size=tex_size;
  tex_data.count=1;
  tex_data.reject=0;
  tex_data.total=gts_surface_face_number(s);
  tex_data.calib=calib;
  tex_data.verbose=verbose;
  tex_data.border_faces = &border_faces;
  if(!blend){
    
    if(num_threads > 1)
      threaded_surface_foreach_face (s, (GtsFunc)texcoord_foreach_face ,
				     &tex_data ,num_threads);
    else
      gts_surface_foreach_face (s, (GtsFunc)texcoord_foreach_face ,&tex_data);
  }else {
    if(num_threads > 1)
      threaded_surface_foreach_face (s, (GtsFunc)texcoord_blend_foreach_face ,
				     &tex_data ,num_threads);
    else
      gts_surface_foreach_face (s, (GtsFunc)texcoord_blend_foreach_face ,&tex_data);
  }
fclose(ffp);
if(verbose)
      printf("\nChecking weird border cases...\n");
 if(!blend){
   if(num_threads > 1)
     threaded_surface_foreach_face (s, (GtsFunc)findborder_foreach_face ,
				    &tex_data ,num_threads);   
   else
     gts_surface_foreach_face (s, (GtsFunc)findborder_foreach_face ,&tex_data );
  
 }else{

  if(num_threads > 1)
     threaded_surface_foreach_face (s, (GtsFunc)findborder_blend_foreach_face ,
				    &tex_data ,num_threads);   
   else
     gts_surface_foreach_face (s, (GtsFunc)findborder_blend_foreach_face ,&tex_data );

 }

 tex_data.count=1;
  tex_data.reject=0;
  tex_data.total =border_faces.size();
  for(int i=0; i < (int) border_faces.size(); i++){
    T_Face *f2 = copy_face(border_faces[i],s);

    if(!blend){
      int idx=find_closet_img_trans(&GTS_FACE(f2)->triangle,bboxTree,camPosePts,back_trans,calib,1);
      if(apply_tex_to_tri(f2,calib,back_trans[idx],idx,tex_size,texMargin))
	tex_data.validCount++;
    }else{
      int idx[NUM_TEX_BLEND_COORDS];
      if(find_blend_img_trans(&GTS_FACE(f2)->triangle,bboxTree,camPosePts,back_trans,calib,idx))
	if(apply_blend_tex_to_tri(f2,calib,back_trans,idx,tex_size,texMargin))
	  tex_data.validCount++;
    }


    gts_surface_remove_face(s,GTS_FACE(border_faces[i]));
    if(verbose)
      tex_add_verbose(tex_data.count++,tex_data.total,tex_data.reject);
  }
  if(verbose)
    printf("\nValid tex %d\n", tex_data.validCount);
 
	   
  
}

OSGExporter::~OSGExporter(){
  map<string, IplImage *>::const_iterator itr;
  for(itr = tex_image_cache.begin(); itr != tex_image_cache.end(); ++itr){
    IplImage *tmp=(*itr).second;
    cvReleaseImage(&tmp); 
  }
  tex_image_cache.clear();
}


