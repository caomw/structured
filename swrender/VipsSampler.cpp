#include "VipsSampler.h"
#include <stdlib.h>
#include <osg/Vec3d>
#include <osg/Vec4>
/*static*/
#include <iostream>
using namespace std;

void VipsSampler::sampleTriCallback(void * param, int x, int y, const osg::Vec3& bar, const osg::Vec3& dx, const osg::Vec3& dy, float coverage)
{
    ((VipsSampler *)param)->sampleTri(x, y, bar, dx, dy, coverage);
}

/*static*/
void VipsSampler::sampleQuadCallback(void * param, int x, int y, const osg::Vec3& bar, const osg::Vec3& dx, const osg::Vec3& dy, float coverage)
{
    ((VipsSampler *)param)->sampleQuad(x, y, bar, dx, dy, coverage);
}


void VipsSampler::setCurrentFace(uint vertexCount, const osg::Vec3 * positions, const osg::Vec3 * normals)
{
    //nvDebugCheck(vertexCount<=4);

    m_positions = positions;
    m_normals = normals;

    for (uint k=0; k<vertexCount; k++)
    {
        m_midedgenormals[k] = normalizeSafe(normals[k] + normals[(k+1)%vertexCount], osg::Vec3(0,0,0), 0);
    }

    if (vertexCount==4) {
        m_midedgenormals[4] = normalizeSafe(m_normals[0]+normals[1]+normals[2]+normals[3], osg::Vec3(0,0,0), 0);
    }
}



void VipsSampler::sampleTri(int x, int y, const osg::Vec3& bar, const osg::Vec3& dx, const osg::Vec3& dy, float coverage)
{


    //if(doublecountmapPtr->count(std::make_pair<int,int>(x,y)) && (*doublecountmapPtr)[std::make_pair<int,int>(x,y)] != triIdx)
      //  return;

    gRect.left=x;
    gRect.top=y;

    if(im_prepare(regOutput,&gRect)){
        fprintf(stderr,"Prepare fail\n");
        exit(-1);
    }

    if(im_prepare(regRange,&gRect)){
        fprintf(stderr,"Prepare fail range blend pass\n");
        exit(-1);
    }
    unsigned int *color=(unsigned int*)IM_REGION_ADDR( regOutput, x, y );
   *color=       255 | 0 << 8 | 0 << 16 | 255 << 24;

return;
    float Cb[]={0.710000,0.650000,0.070000};
    float u=bar.x(), v=bar.y();

    if(u <0 || v <0|| u>1.0 || v > 1.0){
        cout << bar.x() << " "<<bar.y()<<endl;
        printf("Fail %f %f\n",u,v);
        return;
    }
   // cout << u << " " << v<<endl;
    unsigned int *depth=(unsigned int*)IM_REGION_ADDR( regRange, x, y );


    unsigned char dist_curr=reinterpret_cast<unsigned char *>(depth)[idx];
    //Need to handle invalid dist
    float local_r=(dist_curr/255.0)*rmax;

    osg::Vec3d outComp[3];
    for(int j=0; j < 3; j++){
        float W=exp(-local_r*10.0*16.0*Cb[j]);
        // sample the texture and write the color information
        if(j == 0){
            outComp[j]=((texture->sample_nearest(u,v,mipmapL[0])-texture->sample_nearest(u,v,mipmapL[1]))*W);
        }else if (j==1){
            outComp[j]=((texture->sample_nearest(u,v,mipmapL[1])-texture->sample_nearest(u,v,mipmapL[2]))*W);
        }else if(j==2){
            outComp[j]=(texture->sample_nearest(u,v,mipmapL[2])*W);
        }
        // always write the color;
    }
    osg::Vec3 WSum(0.0,0.0,0.0);
    osg::Vec4 r4(0.0,0.0,0.0,0.0);
    for(int j=0; j < 3; j++){
        for(int i=0;i<4; i++){
            unsigned int d=*depth;
            unsigned char oldd[4];

            oldd[0]= d & 0xFF;
            oldd[1] = (d >> 8) & 0xFF;
            oldd[2] = (d >> 16) & 0xFF;
            oldd[3]= (d >> 24) & 0xFF;
            unsigned char dist=oldd[i];
            if(dist ==255)
                continue;
            //Need to handle invalid dist
            float local_r=(dist/255.0)*rmax;
            r4[i]=local_r;
            float W=exp(-local_r*10.0*16.0*Cb[j]);
            WSum[j]+=W;
        }
    }
    outComp[0]=outComp[0]/WSum[0];
    outComp[1]=outComp[1]/WSum[1];
    outComp[2]=outComp[2]/WSum[2];

    osg::Vec3 outP = outComp[0]+outComp[1]+outComp[2];
    //unsigned int *color=(unsigned int*)IM_REGION_ADDR( regOutput, x, y );
   return;
    if(idx != 0)
        return;
    outP=texture->sample_nearest(u,v,0);

    int r,g,b;
    unsigned int c=*color;
    int oldblue,oldgreen,oldred;
    if(USE_BGR){
        oldblue = c & 0xFF;
        oldred = (c >> 16) & 0xFF;
    }else{
        oldred = c & 0xFF;
        oldblue = (c >> 16) & 0xFF;
    }
    oldgreen = (c >> 8) & 0xFF;

    r=(int)(outP.x()*255.0);
    g=(int)(outP.y()*255.0);
    b=(int)(outP.z()*255.0);
    if(USE_BGR)
        c= clamp(b+oldblue,0,255) | clamp(g+oldgreen,0,255) << 8 | clamp(r+oldred,0,255) << 16 | 255 << 24;
    else
        c=  clamp(r+oldred,0,255) | clamp(g+oldgreen,0,255) << 8 | clamp(b+oldblue,0,255) << 16 | 255 << 24;

    *color = c;




#if 0
    //Vector3 normal = normalizeSafe(cross(m_positions[1] - m_positions[0], m_positions[2] - m_positions[0]), Vector3(zero), 0.0f);
    Vector3 linearNormal = normalizeSafe(bar.x() * m_normals[0] + bar.y() * m_normals[1] + bar.z() * m_normals[2], Vector3(zero), 0.0f);
    float u=bar.x(), v=bar.y(), w=bar.z();

    Vector3 normal = normalizeSafe(	u*u*m_normals[0] + v*v*m_normals[1] + w*w*m_normals[2] +
                                    2*(u*v*m_midedgenormals[0] + v*w*m_midedgenormals[1] + w*u*m_midedgenormals[2]), Vector3(zero), 0);

    float mx = max(max(u,v),w);

    m_image.addPixel(coverage * normal, x, y, m_image.normalChannel());

    /*if (m_image.occlusionChannel() != -1)
    {
        float occlusion = sampleOcclusion(position, normal, x, y);
        nvCheck(occlusion >= 0.0f && occlusion <= 1.0f);

        m_image.addPixel(coverage * occlusion, x, y, m_image.occlusionChannel());
    }*/

    m_image.addPixel(coverage, x, y, m_image.coverageChannel());
#endif
    //m_imageMask.setBitAt(x, y);
}


static inline float triangleArea(const osg::Vec2& a, const osg::Vec2& b, const osg::Vec2& c)
{
    osg::Vec2 v0 = a - c;
    osg::Vec2 v1 = b - c;

    return (v0.x() * v1.y() - v0.y() * v1.x());
}

void VipsSampler::sampleQuad(int x, int y, const osg::Vec3& bar, const osg::Vec3& dx, const osg::Vec3& dy, float coverage)
{
#if 0
    nvDebugCheck(isFinite(coverage));

    const float u = bar.x(), U = 1-u;
    const float v = bar.y(), V = 1-v;

    // bilinear position interpolation
    Vector3 position = (U * m_positions[0] + u * m_positions[1]) * V +
            (U * m_positions[3] + u * m_positions[2]) * v;

    m_image.addPixel(coverage * position, x, y, m_image.positionChannel());


    // biquadratic normal interpolation
    Vector3 normal = normalizeSafe(
                (U*U * m_normals[0] + 2*U*u*m_midedgenormals[0] + u*u * m_normals[1]) * V*V +
                (U*U*m_midedgenormals[3] + 2*U*u*m_midedgenormals[4] + u*u*m_midedgenormals[1]) * 2*V*v +
                (U*U * m_normals[3] + 2*U*u*m_midedgenormals[2] + u*u * m_normals[2]) * v*v, Vector3(zero), 0);

    m_image.addPixel(coverage * normal, x, y, m_image.normalChannel());

    /*
    if (m_image.occlusionChannel() != -1)
    {
        // piecewise linear position interpolation
        #if 0
        Vector3 tripos;
        if (u < v)
        {
            float barx = triangleArea(Vector2(1,1), Vector2(0,1), Vector2(u,v));
            float bary = triangleArea(Vector2(0,0), Vector2(1,1), Vector2(u,v));
            float barz = triangleArea(Vector2(0,1), Vector2(0,0), Vector2(u,v));
            nvCheck(equal(1, barx+bary+barz));

            tripos = barx * m_positions[0] + bary * m_positions[1] + barz * m_positions[2];
        }
        else
        {
            float barx = triangleArea(Vector2(1,0), Vector2(1,1), Vector2(u,v));
            float bary = triangleArea(Vector2(1,1), Vector2(0,0), Vector2(u,v));
            float barz = triangleArea(Vector2(0,0), Vector2(1,0), Vector2(u,v));
            nvCheck(equal(1, barx+bary+barz));

            tripos = barx * m_positions[0]  + bary * m_positions[3] + barz * m_positions[2];
        }
        #endif

        float occlusion = sampleOcclusion(position, normal, x, y);
        nvCheck(occlusion >= 0.0f && occlusion <= 1.0f);

        m_image.addPixel(coverage * occlusion, x, y, m_image.occlusionChannel());
    }
    */

    m_image.addPixel(coverage, x, y, m_image.coverageChannel());

    m_imageMask.setBitAt(x, y);
#endif
}