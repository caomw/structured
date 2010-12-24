#ifndef TEXPYRATLAS_H
#define TEXPYRATLAS_H
#include <osgUtil/Optimizer>
#include <string>
#include <osg/State>

class TexPyrAtlas : public osgUtil::Optimizer::TextureAtlasBuilder
{
public:
    typedef long id_type;

    TexPyrAtlas(std::string imgdir);
    void loadSources(std::vector<std::pair<id_type,std::string> > imageList,int sizeIdx);
    int getAtlasId(id_type id);
    osg::ref_ptr<osg::Image> getImage(int index,int sizeIndex);
    int getDownsampleSize(int idx){if(idx > (int) _downsampleSizes.size()) return 0; return _downsampleSizes[idx];}
    osg::ref_ptr<osg::Texture> getTexture(int index,int sizeIndex);
    unsigned int getNumAtlases(){return _atlasList.size();}
    osg::Image *getAtlasByNumber(unsigned int i){
        if(i < _atlasList.size())
            return _atlasList[i]->_image;
        return NULL;
    }
    osg::Matrix getTextureMatrixByID(id_type id);

    void computeImageNumberToAtlasMap(void);
protected:
     std::vector<int> _downsampleSizes;
   osg::State *_state;
   std::vector<osg::ref_ptr<osg::Image> > _images;

   /**
          * Resizes an image using nearest-neighbor resampling. Returns a new image, leaving
          * the input image unaltered.
          *
          * Note. If the output parameter is NULL, this method will allocate a new image and
          * resize into that new image. If the output parameter is non-NULL, this method will
          * assume that the output image is already allocated to the proper size, and will
          * do a resize+copy into that image. In the latter case, it is your responsibility
          * to make sure the output image is allocated to the proper size and with the proper
          * pixel configuration (it must match that of the input image).
          *
          * If the output parameter is non-NULL, then the mipmapLevel is also considered.
          * This lets you resize directly into a particular mipmap level of the output image.
          */
   bool
   resizeImage(const osg::Image* input,
                           unsigned int out_s, unsigned int out_t,
                           osg::ref_ptr<osg::Image>& output,
                           unsigned int mipmapLevel=0 );
   std::string _imgdir;
   std::map<id_type,int> _idToAtlas;
   std::map<Source*,id_type> _sourceToId;
   std::map<id_type,Source*> _idToSource;


};

#endif // TEXPYRATLAS_H
