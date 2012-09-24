/*
 *  plymc.h
 *  filter_plymc
 *
 *  Created by Paolo Cignoni on 10/23/09.
 *  Copyright 2009 ISTI - CNR. All rights reserved.
 *
 */
#ifndef __PLYMC_H__
#define __PLYMC_H__

#ifndef WIN32
#define _int64 long long
#define __int64 long long
#define __cdecl
#endif

#include <cstdio>
#include <time.h>
#include <float.h>
#include <math.h>

#include <locale>
#include <iostream>
//#include <tchar.h>

#include <list>
#include <limits>
#include <vcg/space/index/grid_static_ptr.h>
#include <vcg/simplex/vertex/base.h>
#include <vcg/simplex/face/base.h>
#include <vcg/complex/used_types.h>
#include <vcg/complex/complex.h>

#include <vcg/complex/algorithms/update/position.h>
#include <vcg/complex/algorithms/update/normal.h>
#include <vcg/complex/algorithms/update/quality.h>
#include <vcg/complex/algorithms/update/edges.h>
#include <vcg/complex/algorithms/update/topology.h>
#include <vcg/complex/algorithms/update/flag.h>
#include <vcg/complex/algorithms/update/bounding.h>
#include <vcg/math/histogram.h>
#include <vcg/complex/algorithms/clean.h>
#include <wrap/io_trimesh/import.h>
#include <wrap/io_trimesh/export_ply.h>
#include <wrap/ply/plystuff.h>

#include <vcg/complex/algorithms/create/marching_cubes.h>
#include <vcg/complex/algorithms/create/extended_marching_cubes.h>
#include "trivial_walker.h"
// local optimization
#include <vcg/complex/algorithms/local_optimization.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric.h>

#include <vcg/simplex/edge/base.h>
#include <stdarg.h>
#include "volume.h"
#include "tri_edge_collapse_mc.h"
#include <osg/Timer>
typedef bool CallBackPosTotal(const int pos, const int total,unsigned long long tick,const char * str );

namespace vcg {
    namespace tri {
      
      // Simple prototype for later use...
      template<class MeshType>
      void MCSimplify( MeshType &m, float perc, bool preserveBB=true, vcg::CallBackPos *cb=0);
              
              

  template < class SMesh, class MeshProvider>
  class PlyMC
  {
      public:
    class MCVertex;
    class MCEdge;
    class MCFace;

    class MCUsedTypes: public vcg::UsedTypes <vcg::Use<MCVertex>::template AsVertexType,
                                              vcg::Use<MCEdge  >::template AsEdgeType,
                                              vcg::Use<MCFace  >::template AsFaceType >{};

    class MCVertex  : public Vertex< MCUsedTypes, vertex::Coord3f, vertex::Color4b, vertex::Mark, vertex::VFAdj, vertex::BitFlags, vertex::Qualityf>{};
    class MCEdge : public Edge<MCUsedTypes,edge::VertexRef> {
    public:
      inline MCEdge() {};
      inline MCEdge( MCVertex * v0, MCVertex * v1){this->V(0) = v0; this->V(1) = v1; };
      static inline MCEdge OrderedEdge(MCVertex* v0,MCVertex* v1){
       if(v0<v1) return MCEdge(v0,v1);
       else return MCEdge(v1,v0);
      }
    };

    class MCFace    : public Face< MCUsedTypes, face::InfoOcf, face::VertexRef, face::FFAdjOcf, face::VFAdjOcf, face::BitFlags> {};
    class MCMesh	: public vcg::tri::TriMesh< std::vector< MCVertex>, face::vector_ocf< MCFace > > {};


	//******************************************
	//typedef Voxel<float> Voxelf;
	typedef Voxelfc Voxelf;
	//******************************************

	class Parameter
	{
	public:
		Parameter()
		{
            NCell=10000;
            WideNum= 3;
            WideSize=0;
            VoxSize=0;
            IPosS=Point3i(0,0,0);  // SubVolume Start
            IPosE=Point3i(0,0,0);  // SubVolume End
            IPosB=Point3i(0,0,0);  // SubVolume to restart from in lexicographic order (useful for crashes)
            //IPos=Point3i(0,0,0);
            IDiv=Point3i(1,1,1);
            VerboseLevel=0;
            SliceNum=1;
            FillThr=12;
            ExpAngleDeg=30;
            SmoothNum=1;
            RefillNum=1;
            IntraSmoothFlag = false;
            QualitySmoothAbs = 0.0f; //  0 means un-setted value.
            QualitySmoothVox = 3.0f; // expressed in voxel
            OffsetFlag=false;
            OffsetThr=-3;
            GeodesicQualityFlag=true;
            PLYFileQualityFlag=false;
            SaveVolumeFlag=false;
            SafeBorder=1;
            CleaningFlag=false;
            SimplificationFlag=false;
            VertSplatFlag=false;
            MergeColor=false;
            basename = "plymcout";
		}

		int NCell;
		int WideNum;
		float WideSize;
		float VoxSize;
		Point3i IPosS;  // SubVolume Start
		Point3i IPosE;  // SubVolume End
		Point3i IPosB;  // SubVolume to restart from in lexicographic order (useful for crashes)
                //Point3i IPos;
		Point3i IDiv;
		int VerboseLevel;
		int SliceNum;
		int FillThr;
		float ExpAngleDeg;
		int SmoothNum;
		int RefillNum;
		bool IntraSmoothFlag; 
		float QualitySmoothAbs; //  0 means un-setted value.
		float QualitySmoothVox; // expressed in voxel
		bool OffsetFlag;
		float OffsetThr;
		bool GeodesicQualityFlag;
		bool PLYFileQualityFlag;
		bool SaveVolumeFlag;
		int SafeBorder;
		bool CleaningFlag;
		bool SimplificationFlag;
		bool VertSplatFlag;
		bool MergeColor;
		std::string basename;
    std::vector<std::string> OutNameVec;
    std::vector<std::string> OutNameSimpVec;
  }; //end Parameter class
	
/// PLYMC Data

    MeshProvider MP;
    Parameter p;
     std::vector< std::vector<std::vector<Volume<Voxelf> > > >vVV;

/// PLYMC Methods

bool InitMesh(Volume<Voxelf> &VV,SMesh &m, const char *filename, Matrix44f Tr)
{
    typename SMesh::VertexIterator vi;
    int loadmask;
    int ret = tri::io::Importer<SMesh>::Open(m,filename,loadmask);
    tri::Clean<SMesh>::FlipMesh(m);

    if(ret)
    {
      printf("Error: unabe to open mesh '%s'",filename);
      return false;
    }

    if(p.VertSplatFlag)
    {
      if(!(loadmask & tri::io::Mask::IOM_VERTNORMAL))
      {
        printf("Error, pointset MUST have normals");
        exit(-1);
      }
      else printf("Ok Pointset has normals\n");
      for(vi=m.vert.begin(); vi!=m.vert.end();++vi)
        if(math::Abs(SquaredNorm((*vi).N())-1.0)>0.0001)
        {
        printf("Error: mesh has not per vertex normalized normals\n");
        return false;
      }

      if(!(loadmask & tri::io::Mask::IOM_VERTQUALITY))
        tri::UpdateQuality<SMesh>::VertexConstant(m,0);
      tri::UpdateNormals<SMesh>::PerVertexMatrix(m,Tr);
      //if(!(loadmask & tri::io::Mask::IOM_VERTCOLOR))
      //  saveMask &= ~tri::io::Mask::IOM_VERTCOLOR;
    }
    else // processing for triangle meshes
    {
      if(p.CleaningFlag){
          int dup = tri::Clean<SMesh>::RemoveDuplicateVertex(m);
          int unref =  tri::Clean<SMesh>::RemoveUnreferencedVertex(m);
          printf("Removed %i duplicates and %i unref",dup,unref);
      }

      tri::UpdateNormals<SMesh>::PerVertexNormalizedPerFaceNormalized(m);
      if(p.GeodesicQualityFlag) {
          tri::UpdateTopology<SMesh>::VertexFace(m);
          tri::UpdateFlags<SMesh>::FaceBorderFromVF(m);
          tri::UpdateQuality<SMesh>::VertexGeodesicFromBorder(m);
        }
    }

    tri::UpdatePosition<SMesh>::Matrix(m,Tr,false);
    tri::UpdateBounding<SMesh>::Box(m);
    //printf("Init Mesh %s (%ivn,%ifn)\n",filename,m.vn,m.fn);

    for(vi=m.vert.begin(); vi!=m.vert.end();++vi)
        VV.Interize((*vi).P());
    return true;
  }

// This function add a mesh (or a point cloud to the volume)
// the point cloud MUST have normalized vertex normals.

bool AddMeshToVolumeM(Volume<Voxelf>  &VV,SMesh &m, std::string meshname, const double w )
{
    typename SMesh::VertexIterator vi;
    typename SMesh::FaceIterator fi;
    if(!m.bbox.Collide(VV.SubBoxSafe)) return false;
    size_t found =meshname.find_last_of("/\\");
    std::string shortname = meshname.substr(found+1);

    Volume <Voxelf> B;
    B.Init(VV);


    bool res=false;
    double quality=0;

  // Now add the mesh to the volume
    if(!p.VertSplatFlag)
    {
            float minq=std::numeric_limits<float>::max(), maxq=-std::numeric_limits<float>::max();
            // Calcolo range qualita geodesica PER FACCIA come media di quelle per vertice
            for(fi=m.face.begin(); fi!=m.face.end();++fi){
                (*fi).Q()=((*fi).V(0)->Q()+(*fi).V(1)->Q()+(*fi).V(2)->Q())/3.0f;
                minq=std::min((*fi).Q(),minq);
                maxq=std::max((*fi).Q(),maxq);
            }


            // La qualita' e' inizialmente espressa come distanza assoluta dal bordo della mesh
            //printf("Q [%4.2f  %4.2f] \n",minq,maxq);
            bool closed=false;
            if(minq==maxq) closed=true;  // se la mesh e' chiusa la  ComputeGeodesicQuality mette la qualita a zero ovunque
            // Classical approach: scan each face
            int tt0=clock();
            //printf("---- Face Rasterization");
            for(fi=m.face.begin(); fi!=m.face.end();++fi)
                {
                    if(closed || (p.PLYFileQualityFlag==false && p.GeodesicQualityFlag==false)) quality=1.0;
                    else quality=w*(*fi).Q();
                    if(quality)
                            res |= B.ScanFace((*fi).V(0)->P(),(*fi).V(1)->P(),(*fi).V(2)->P(),quality,(*fi).N());
                }
          //  printf(" : %li\n",clock()-tt0);

    } else
    {	// Splat approach add only the vertices to the volume
        printf("Vertex Splatting\n");
        for(vi=m.vert.begin();vi!=m.vert.end();++vi)
                {
                    if(p.PLYFileQualityFlag==false) quality=1.0;
                    else quality=w*(*vi).Q();
                    if(quality)
                        res |= B.SplatVert((*vi).P(),quality,(*vi).N(),(*vi).C());
                }
    }
    if(!res) return false;

    int vstp=0;
    if(p.VerboseLevel>0) {
      B.SlicedPPM(shortname.c_str(),std::string(SFormat("%02i",vstp)).c_str(),p.SliceNum	);
      B.SlicedPPMQ(shortname.c_str(),std::string(SFormat("%02i",vstp)).c_str(),p.SliceNum	);
      vstp++;
    }
    for(int i=0;i<p.WideNum;++i) {
    B.Expand(math::ToRad(p.ExpAngleDeg));
        if(p.VerboseLevel>1) B.SlicedPPM(shortname.c_str(),SFormat("%02ie",vstp++),p.SliceNum	);
        B.Refill(p.FillThr);
        if(p.VerboseLevel>1) B.SlicedPPM(shortname.c_str(),SFormat("%02if",vstp++),p.SliceNum	);
        if(p.IntraSmoothFlag)
        {
            Volume <Voxelf> SM;
            SM.Init(VV);
            SM.CopySmooth(B,1,p.QualitySmoothAbs);
            B=SM;
            if(p.VerboseLevel>1) B.SlicedPPM(shortname.c_str(),SFormat("%02is",vstp++),p.SliceNum	);
//			if(VerboseLevel>1) B.SlicedPPMQ(shortname,SFormat("%02is",vstp),SliceNum	);
        }
    }
    if(p.SmoothNum>0)
        {
            Volume <Voxelf> SM;
            SM.Init(VV);
            SM.CopySmooth(B,1,p.QualitySmoothAbs);
            B=SM;
            if(p.VerboseLevel>1) B.SlicedPPM(shortname.c_str(),SFormat("%02isf",vstp++),p.SliceNum	);
        }
    VV.Merge(B);
    if(p.VerboseLevel>0) VV.SlicedPPMQ(std::string("merge_").c_str(),shortname.c_str(),p.SliceNum	);
    return true;
}

void Process(CallBackPosTotal *cb=0)
{
    unsigned long long startTick=osg::Timer::instance()->tick();
    printf("bbox scanning...\n"); fflush(stdout);
    Matrix44f Id; Id.SetIdentity();
    MP.InitBBox();
    printf("Completed BBox Scanning                   \n");
    Box3f fullb = MP.fullBB();
    assert (!fullb.IsNull());
    assert (!fullb.IsEmpty());
    // Calcolo gridsize
    Point3i gridsize;
    Point3f voxdim;
    fullb.Offset(fullb.Diag() * 0.1 );

    int saveMask=0;
   if(p.MergeColor) saveMask |= tri::io::Mask::IOM_VERTCOLOR ;

    voxdim = fullb.max - fullb.min;

    int TotAdd=0,TotMC=0,TotSav=0;
    // if kcell==0 the number of cells is computed starting from required voxel size;
    __int64 cells;
    if(p.NCell>0) cells = (__int64)(p.NCell)*(__int64)(1000);
    else cells = (__int64)(voxdim[0]/p.VoxSize) * (__int64)(voxdim[1]/p.VoxSize) *(__int64)(voxdim[2]/p.VoxSize) ;
    Box3i globalBox;
    {
        Volume<Voxelf> B; // local to this small block

        Box3f fullbf; fullbf.Import(fullb);
        B.Init(cells,fullbf,p.IDiv,p.IPosS);
        B.Dump(stdout);
        if(p.WideSize>0) p.WideNum=p.WideSize/B.voxel.Norm();
    globalBox=B.SubPart;
        // Now the volume has been determined; the quality threshold in absolute units can be computed
        if(p.QualitySmoothAbs==0)
            p.QualitySmoothAbs= p.QualitySmoothVox * B.voxel.Norm();
    }


    bool res=false;
vVV.resize(p.IDiv[0]);
for(int i=0; i<vVV.size(); i++){
    vVV[i].resize(p.IDiv[1]);
    for(int j=0; j<vVV[i].size(); j++)
        vVV[i][j].resize(p.IDiv[2]);

}
//#pragma omp parallel for
    for(int xx=p.IPosS[0];xx<=p.IPosE[0];++xx)
      for(int yy=p.IPosS[1];yy<=p.IPosE[1];++yy)
        for(int zz=p.IPosS[2];zz<=p.IPosE[2];++zz)
          if((zz+(yy*p.IDiv[2])+(xx*p.IDiv[2]*p.IDiv[1])) >=
	     (p.IPosB[2]+(p.IPosB[1]*p.IDiv[2])+(p.IPosB[0]*p.IDiv[2]*p.IDiv[1]))) // skip until IPos >= IPosB
	      {
                printf("----------- SubBlock %2i %2i %2i ----------\n",xx,yy,zz);
		//Volume<Voxelf> B;
                Volume<Voxelf> &VV =vVV[xx][yy][zz];
                int t0=clock();

		Box3f fullbf; fullbf.Import(fullb);
        //VV.DeltaVoxelSafe=1;
        Point3i IPos;
        IPos[0]=xx;
        IPos[1]=yy;
        IPos[2]=zz;

                VV.Init(cells,fullbf,p.IDiv,IPos);
		printf("\n\n --------------- Allocated subcells. %i\n",VV.Allocated());

		std::string filename=p.basename;
		if(p.IDiv!=Point3i(1,1,1))
		{
		  std::string subvoltag;
		  VV.GetSubVolumeTag(subvoltag);
		  filename+=subvoltag;
		}
		/********** Grande loop di scansione di tutte le mesh *********/
		for(int i=0;i<MP.size();++i)
		{
		  Box3f bbb= MP.bb(i);
      /**********************/
      cb((i+1),MP.size(),startTick,"Vol");
      /**********************/
      // if bbox of mesh #i is part of the subblock, then process it
#pragma omp critical
      if(bbb.Collide(VV.SubBoxSafe))
		  {
		    SMesh *sm;
		    if(!MP.Find(i,sm) )
		    {
          res = InitMesh(VV,*sm,MP.MeshName(i).c_str(),MP.Tr(i));
          if(!res)
          {
            printf("Failed Init of mesh %s",MP.MeshName(i).c_str());
            //break;
          }
		    }
                    res |= AddMeshToVolumeM(VV,*sm, MP.MeshName(i),MP.W(i));
		  }
		}

		//B.Normalize(1);
		printf("End Scanning\n");
		if(p.OffsetFlag)
		{
		    VV.Offset(p.OffsetThr);
		    if (p.VerboseLevel>0)
		    {
          VV.SlicedPPM("finaloff","__",p.SliceNum);
          VV.SlicedPPMQ("finaloff","__",p.SliceNum);
		    }
		}
		//if(p.VerboseLevel>1) VV.SlicedPPM(filename.c_str(),SFormat("_%02im",i),p.SliceNum	);

		for(int i=0;i<p.RefillNum;++i)
		{
                  //VV.Refill(3,6);
		  if(p.VerboseLevel>1) VV.SlicedPPM(filename.c_str(),SFormat("_%02imsr",i),p.SliceNum	);
		  //if(VerboseLevel>1) VV.SlicedPPMQ(filename,SFormat("_%02ips",i++),SliceNum	);
		}

		for(int i=0;i<p.SmoothNum;++i)
		{
		  Volume <Voxelf> SM;
		  SM.Init(VV);
		  printf("%2i/%2i: ",i,p.SmoothNum);
		  SM.CopySmooth(VV,1,p.QualitySmoothAbs);
		  VV=SM;
                //  VV.Refill(3,6);
		  if(p.VerboseLevel>1) VV.SlicedPPM(filename.c_str(),SFormat("_%02ims",i),p.SliceNum	);
		}

		int t1=clock();  //--------
		TotAdd+=t1-t0;
		printf("Extracting surface...\r");
		if (p.VerboseLevel>0)
		{
		    VV.SlicedPPM("final","__",p.SliceNum);
		    VV.SlicedPPMQ("final","__",p.SliceNum);
		}
                //VV.Write("test",0,0);
		//MCMesh me;
		//

            }
int cnt=0;
    for(int xx=p.IPosS[0];xx<=p.IPosE[0];++xx)
      for(int yy=p.IPosS[1];yy<=p.IPosE[1];++yy)
        for(int zz=p.IPosS[2];zz<=p.IPosE[2];++zz)
          if((zz+(yy*p.IDiv[2])+(xx*p.IDiv[2]*p.IDiv[1])) >=
             (p.IPosB[2]+(p.IPosB[1]*p.IDiv[2])+(p.IPosB[0]*p.IDiv[2]*p.IDiv[1]))) // skip until IPos >= IPosB
              {
        Volume<Voxelf> &VV =vVV[xx][yy][zz];
        Box3i ibox;

        for(int xxx=p.IPosS[0];xxx<=p.IPosE[0];++xxx)
            for(int yyy=p.IPosS[1];yyy<=p.IPosE[1];++yyy){
            if(xxx==xx && yyy==yy)
                continue;

            Box3i SubPartSafe=vVV[xxx][yyy][zz].SubPartSafe;
            if(!SubPartSafe.Collide(VV.SubPartSafe))
                continue;
            ibox.min[0] = std::max(SubPartSafe.min[0],VV.SubPartSafe.min[0]);
            ibox.min[1] = std::max(SubPartSafe.min[1],VV.SubPartSafe.min[1]);
            ibox.min[2] = std::max(SubPartSafe.min[2],VV.SubPartSafe.min[2]);

            ibox.max[0] = std::min(SubPartSafe.max[0],VV.SubPartSafe.max[0]);
            ibox.max[1] = std::min(SubPartSafe.max[1],VV.SubPartSafe.max[1]);
            ibox.max[2] = std::min(SubPartSafe.max[2],VV.SubPartSafe.max[2]);
           // ibox=globalBox;

          /*  printf("%d %d %d -- %d %d %d\n",ibox.min[0],ibox.min[1],ibox.min[2],
                   ibox.max[0],ibox.max[1],ibox.max[2]);
            printf("A %d %d %d -- %d %d %d\n",SubPartSafe.min[0],SubPartSafe.min[1],SubPartSafe.min[2],
                   SubPartSafe.max[0],SubPartSafe.max[1],SubPartSafe.max[2]);
            printf("B %d %d %d -- %d %d %d\n",VV.SubPartSafe.min[0],VV.SubPartSafe.min[1],VV.SubPartSafe.min[2],
                   VV.SubPartSafe.max[0],VV.SubPartSafe.max[1],VV.SubPartSafe.max[2]);*/
            for(int xxxx=ibox.min[0];xxxx<=ibox.max[0];++xxxx)
              for(int yyyy=ibox.min[1];yyyy<=ibox.max[1];++yyyy)
                  for(int zzzz=ibox.min[2];zzzz<=ibox.max[2];++zzzz){
              //  printf("%d %d %d\n",xxxx,yyyy,zzzz);
               if(vVV[xxx][yyy][zz].Val(xxxx,yyyy,zzzz)!=VV.Val(xxxx,yyyy,zzzz)){
                           // printf("%f %f\n",vVV[xxx][yyy][zz].Val(xxxx,yyyy,zzzz),VV.Val(xxxx,yyyy,zzzz));
                        if(vVV[xxx][yyy][zz].Val(xxxx,yyyy,zzzz) == 1000.000)
                            vVV[xxx][yyy][zz].V(xxxx,yyyy,zzzz).Set(VV.V(xxxx,yyyy,zzzz));
                        else if(VV.Val(xxxx,yyyy,zzzz) == 1000.000)
                            VV.V(xxxx,yyyy,zzzz).Set(vVV[xxx][yyy][zz].V(xxxx,yyyy,zzzz));
                        else{
                            vVV[xxx][yyy][zz].V(xxxx,yyyy,zzzz).Blend(VV.V(xxxx,yyyy,zzzz),0.5);
                          VV.V(xxxx,yyyy,zzzz).Set(  vVV[xxx][yyy][zz].V(xxxx,yyyy,zzzz));

                        }
                }
              //  vVV[xxx][yyy][zz].V(xxxx,yyyy,zzzz).Set(VV.V(xxxx,yyyy,zzzz));
            //    VV.V(xxxx,yyyy,zzzz).SetB(false);


            }
        }
  /*      std::string filename="final";
        if(p.IDiv!=Point3i(1,1,1))
        {
          std::string subvoltag;
          VV.GetSubVolumeTag(subvoltag);
          filename+=subvoltag;
        }
        VV.SlicedPPM(filename.c_str(),"__",1);
        VV.SlicedPPMQ(filename.c_str(),"__",1);

        VV.Dump(stdout);*/

                printf("----------- Equalizing corner SubBlock %2i %2i %2i ----------\n",xx,yy,zz);
                //Volume<Voxelf> B;

            }
//#pragma omp parallel for

    for(int xx=p.IPosS[0];xx<=p.IPosE[0];++xx)
      for(int yy=p.IPosS[1];yy<=p.IPosE[1];++yy)
        for(int zz=p.IPosS[2];zz<=p.IPosE[2];++zz)
          if((zz+(yy*p.IDiv[2])+(xx*p.IDiv[2]*p.IDiv[1])) >=
             (p.IPosB[2]+(p.IPosB[1]*p.IDiv[2])+(p.IPosB[0]*p.IDiv[2]*p.IDiv[1]))) // skip until IPos >= IPosB
              {
        Volume<Voxelf> &VV =vVV[xx][yy][zz];

        std::string filename=p.basename;
      //  if(p.IDiv!=Point3i(1,1,1))
        {
          std::string subvoltag;
          VV.GetSubVolumeTag(subvoltag);
          filename+=subvoltag;
        }
		MCMesh me;
		if(res)
		{
		  typedef vcg::tri::TrivialWalker<MCMesh, Volume <Voxelf> >	Walker;
		  typedef vcg::tri::MarchingCubes<MCMesh, Walker>             MarchingCubes;
		  //typedef vcg::tri::ExtendedMarchingCubes<MCMesh, Walker> ExtendedMarchingCubes;

		  Walker walker;
		  MarchingCubes	mc(me, walker);
		  Box3i currentSubBox=VV.SubPartSafe;
		  Point3i currentSubBoxRes=VV.ssz;
      /**********************/
      cb(50,50,0,"Step 2: Marching Cube...");
      /**********************/
      walker.BuildMesh(me,VV,mc,currentSubBox,currentSubBoxRes);

      typename MCMesh::VertexIterator vi;
		  Box3f bbb; bbb.Import(VV.SubPart);
		  for(vi=me.vert.begin();vi!=me.vert.end();++vi)
		  {
		      if(!bbb.IsIn((*vi).P()))
			  vcg::tri::Allocator< MCMesh >::DeleteVertex(me,*vi);
		      VV.DeInterize((*vi).P());
		  }
		  typename MCMesh::FaceIterator fi;
		  for (fi = me.face.begin(); fi != me.face.end(); ++fi)
		  {
		      if((*fi).V(0)->IsD() || (*fi).V(1)->IsD() || (*fi).V(2)->IsD() )
			  vcg::tri::Allocator< MCMesh >::DeleteFace(me,*fi);
		      else std::swap((*fi).V1(0), (*fi).V2(0));
		  }

		  int t2=clock();  //--------
//		  TotMC+=t2-t1;
		  if(me.vn >0 || me.fn >0)
		  {
        p.OutNameVec.push_back(filename+std::string(".ply"));
        tri::io::ExporterPLY<MCMesh>::Save(me,p.OutNameVec.back().c_str(),saveMask);


        if(p.SimplificationFlag)
        {
            MCMesh me2;
            tri::io::ImporterPLY<MCMesh>::Open(me2,p.OutNameVec.back().c_str(),saveMask);
          /**********************/
          cb(50,50,0,"Step 3: Simplify mesh...");
          /**********************/
          p.OutNameSimpVec.push_back(filename+std::string(".d.ply"));
          me2.face.EnableVFAdjacency();
          me2.face.EnableFFAdjacency();

          vcg::tri::UpdateTopology<MCMesh>::VertexFace(me2);
          vcg::tri::UpdateTopology<MCMesh>::FaceFace(me2);

          tri::Clean<MCMesh>::RemoveNonManifoldVertex(me2);
          tri::Clean<MCMesh>::RemoveNonManifoldFace(me2);
          tri::Clean<MCMesh>::RemoveTVertexByFlip(me2,20,true);
          tri::Clean<MCMesh>::RemoveFaceFoldByFlip(me2);
           MCSimplify<MCMesh>(me2, VV.voxel[0]/4.0);
          tri::Allocator<MCMesh>::CompactFaceVector(me2);
        //  me2.face.EnableFFAdjacency();


          tri::Clean<MCMesh>::RemoveTVertexByFlip(me2,20,true);
          tri::Clean<MCMesh>::RemoveFaceFoldByFlip(me2);
          tri::io::ExporterPLY<MCMesh>::Save(me2,p.OutNameSimpVec.back().c_str(),saveMask);
        }
		  }
		  int t3=clock();  //--------
		  TotSav+=t3-t2;

		}

		printf("Mesh Saved '%s':  %8d vertices, %8d faces                   \n",(filename+std::string(".ply")).c_str(),me.vn,me.fn);
		printf("Adding Meshes %8i\n",TotAdd);
		printf("MC            %8i\n",TotMC);
		printf("Saving        %8i\n",TotSav);
		printf("Total         %8i\n",TotAdd+TotMC+TotSav);
	      }
	    else
	      {
                 printf("----------- skipping SubBlock %2i %2i %2i ----------\n",xx,yy,zz);
	      }
}


}; //end PlyMC class




  template < class MeshType>
           class PlyMCTriEdgeCollapse: public MCTriEdgeCollapse< MeshType, PlyMCTriEdgeCollapse<MeshType> > {
                          public:
                          typedef  MCTriEdgeCollapse< MeshType,  PlyMCTriEdgeCollapse  > MCTEC;
              typedef  typename  MeshType::VertexType::EdgeType EdgeType;
              inline PlyMCTriEdgeCollapse(  const EdgeType &p, int i) :MCTEC(p,i){}
   };



template<   class MeshType>
void MCSimplify( MeshType &m, float absoluteError, bool preserveBB, vcg::CallBackPos *cb)
{
    typedef PlyMCTriEdgeCollapse<MeshType> MyColl;

    tri::UpdateBounding<MeshType>::Box(m);
    tri::UpdateTopology<MeshType>::VertexFace(m);
    vcg::LocalOptimization<MeshType> DeciSession(m);
    MyColl::bb()=m.bbox;
    MyColl::preserveBBox()=preserveBB;
    if(absoluteError==0)
    {
      // guess the mc side.
      // In a MC mesh the vertices are on the egdes of the cells. and the edges are (mostly) on face of the cells.
      // If you have  2 vert over the same face xy they share z

      std::vector<float> ZSet;

      typename MeshType::FaceIterator fi;
      for(fi = m.face.begin();fi!=m.face.end();++fi)
        if(!(*fi).IsD())
        {
         Point3f v0=(*fi).V(0)->P();
         Point3f v1=(*fi).V(1)->P();
         Point3f v2=(*fi).V(2)->P();
         if(v0[2]==v1[2] && v0[1]!=v1[1] && v0[0]!=v1[0]) ZSet.push_back(v0[2]);
         if(v0[2]==v2[2] && v0[1]!=v1[1] && v2[0]!=v2[0]) ZSet.push_back(v0[2]);
         if(v1[2]==v2[2] && v1[1]!=v1[1] && v2[0]!=v2[0]) ZSet.push_back(v0[2]);
         if(ZSet.size()>100) break;
       }
      std::sort(ZSet.begin(),ZSet.end());
      std::vector<float>::iterator lastV = std::unique(ZSet.begin(),ZSet.end());
      ZSet.resize(lastV-ZSet.begin());
      float Delta=0;
      for(size_t i = 0; i< ZSet.size()-1;++i)
      {
        Delta = std::max(ZSet[i+1]-ZSet[i],Delta);
        //qDebug("%f",Delta);
      }
      absoluteError= Delta/4.0f;
    }
    //qDebug("Simplifying at absoluteError=%f",absoluteError);

    float TargetError = absoluteError;
    char buf[1024];
    DeciSession.template Init< MyColl > ();

    MyColl::areaThr()=TargetError*TargetError;
    DeciSession.SetTimeBudget(1.0f);
    if(TargetError < std::numeric_limits<float>::max() ) DeciSession.SetTargetMetric(TargetError);
    while(DeciSession.DoOptimization() && DeciSession.currMetric < TargetError)
      {
        sprintf(buf,"Simplyfing %7i err %9g \r",m.fn,DeciSession.currMetric);
        if (cb) cb(int(100.0f*DeciSession.currMetric/TargetError),buf);
      }
}


} // end namespace tri
} // end namespace vcg

#endif
