/*
 * StereoEngine.h
 * Copyright (C) Brandon Wampler 2009 <bwampler@purdue.edu>
 *
 * StereoEngine.h is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * StereoEngine.h is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef StereoEngine_H
#define StereoEngine_H

#include "cv.h"
#include <cvaux.h>
#include "cxmisc.h"
#include "highgui.h"
#include <vector>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <ctype.h>
#include <iostream>
#include <osg/StateAttribute>
#include <osg/BlendFunc>
#include "pthread.h"
#include <osgViewer/Viewer>
#ifndef USE_AUV_LIBS

#include "configFile.h"
#else
#include "auv_config_file.hpp"
using libplankton::Config_File;

#include "auv_stereo_corner_finder.hpp"
#include "auv_stereo_dense.hpp"
using libsnapper::MatchStats;


#endif
#include <list>
//#include <opencv2/legacy/legacy.hpp>
#include "calibFile.h"

#define AUV_NO_Z_GUESS -100.0
#ifndef USE_AUV_LIBS

class MatchStats{
 public:
    unsigned int total_init_feat;
  unsigned int total_matched_feat;
  unsigned int total_accepted_feat;
  unsigned int total_tracking_fail;
  unsigned int total_tri_fail;

  unsigned int total_epi_fail;
  MatchStats(void) : total_init_feat(0),total_matched_feat(0),
      total_accepted_feat(0),total_tracking_fail(0),total_tri_fail(0),
      total_epi_fail(0)
  { }
};
#endif
typedef enum {
    STEREO_OK,
    FAIL_FEAT_THRESH,
    FAIL_TRI_EDGE_THRESH,
    FAIL_OTHER
}StereoStatusFlag;
class StereoEngine
{
public:

#if USE_AUV_LIBS
    libsnapper::Stereo_Feature_Finder *finder;
    unsigned int frame_id;
    libsnapper::Stereo_Dense *sdense;
    libsnapper::Stereo_Calib *_auv_stereo_calib;
    double thresh_per_rejected_output_debug;
    int minFeatPerFrameThresh;
#define KRED  "\x1B[31m"
#define KNRM  "\x1B[0m"
#define KBGRDRED "\033[41m"
#define KFRGWHITE "\033[37m"

#endif
    static const int winSize = 10;
    static const int maxCount = 20;
    static const double quality = 0.01;
    static const double minDistance = 10;
    pthread_t viewerThread;
    osgViewer::Viewer viewer;

    // Dynamically Allocated (Must Release Memory)
    IplImage *temp,*eig, /**leftImage, *rightImage, *leftGreyR, *rightGreyR,*/*leftPyr,*rightPyr;
 //*eig, , *threeD, *leftImageR;
    //CvPoint2D32f *points[2];
//    CvMat *Q, /**F,*/ *R1, *R2, *P1, *P2, *M1, *M2, *D1, *D2,
  //  *mx1, *my1, *mx2, *my2, *pair, *R, *T, *E, *disp, *vdisp, *img1r, *img2r, *realDisp;
    CvCalibFilter m_CalibFilter;
    CvStereoBMState *BMState;

    // Links (Don't Deallocate)
  //  IplImage *frameL, *frameR;
    StereoCalib _calib;

    // Statically Allocated
    int pointCount;
    CvSize imageSize;
    std::vector<std::string> imageNames[2];
public:
    StereoEngine(const StereoCalib &calib,Config_File &recon,double edgethresh,double max_triangulation_len,int max_feature_count,
                 double min_feat_dist,double feat_quality,int tex_size,OpenThreads::Mutex &mutex,bool use_dense_stereo,bool pause_after_each_frame);
    virtual ~StereoEngine();
    void captureCalibrationImages(int number);
    static void stereoCalibrate(std::string imageList, int nx, int ny, int useUncalibrated);
    void findDisparity();
    void displayDisparity();
    void sparseDepth(IplImage *leftGrey,IplImage *rightGrey,const int MAX_COUNT, std::list<osg::Vec3> &points,double zguess);
 //   void denseDepth(const int MAX_COUNT, matrix<double> &bodyCoord, matrix<double> &colors, int &sparseFeatureCount);

    void reprojectTo3d();
    void drawPointCloud();
    osg::StateSet* makeStateSet(float size);
    void loadMatrices();
    void capture();
    void drawPoints(CvArr* image, CvPoint2D32f points[], int count);
    void calculateOpticalFlow();
    void displayOpticalFlow();
    StereoStatusFlag processPair(const std::string basedir,const std::string left_file_name,const std::string &right_file_name ,const osg::Matrix &mat,osg::BoundingBox &bbox,MatchStats &stats,const double feature_depth_guess,bool cache_img,bool use_cached);
    double edgethresh;
    double max_triangulation_len;
    int max_feature_count;
    double min_feat_dist;
    double feat_quality;
    int tex_size;
    bool verbose;
    bool use_dense_stereo;
    bool display_debug_images;
    bool pause_after_each_frame;
    OpenThreads::Mutex &_osgDBMutex;
    Config_File &_recon;
};

class is_same_vec3_xy
{
public:
    bool operator() (const osg::Vec3 &first, const osg::Vec3 &second)
    {
        return(first.x() ==second.x() && first.y() == second.y());
    }
};
bool get_stereo_pair(
    const std::string contents_dir_name,
    IplImage     *&left_image,
    IplImage     *&right_image,
    IplImage *&color_image,
    const std::string        &left_image_name,
    const std::string        &right_image_name );
bool checkmkdir(std::string dir);
double get_time( void );
void cacheImage(IplImage *img,std::string name,int tex_size);
#endif

// vim:ts=4:sw=4
