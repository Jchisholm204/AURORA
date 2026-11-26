#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <signal.h>
#if USE_MPI
#include <mpi.h>
#endif
#include "lulesh.h"
#include "lulesh-checkpoint.h"

/* Helper function for converting strings to ints, with error checking */
int StrToInt(const char *token, int *retVal)
{
   const char *c ;
   char *endptr ;
   const int decimal_base = 10 ;

   if (token == NULL)
      return 0 ;
   
   c = token ;
   *retVal = (int)strtol(c, &endptr, decimal_base) ;
   if((endptr != c) && ((*endptr == ' ') || (*endptr == '\0')))
      return 1 ;
   else
      return 0 ;
}

static void PrintCommandLineOptions(char *execname, int myRank)
{
   if (myRank == 0) {

      printf("Usage: %s [opts]\n", execname);
      printf(" where [opts] is one or more of:\n");
      printf(" -q              : quiet mode - suppress all stdout\n");
      printf(" -i <iterations> : number of cycles to run\n");
      printf(" -s <size>       : length of cube mesh along side\n");
      printf(" -r <numregions> : Number of distinct regions (def: 11)\n");
      printf(" -b <balance>    : Load balance between regions of a domain (def: 1)\n");
      printf(" -c <cost>       : Extra cost of more expensive regions (def: 1)\n");
      printf(" -f <numfiles>   : Number of files to split viz dump into (def: (np+10)/9)\n");
      printf(" -p              : Print out progress\n");
      printf(" -v              : Output viz file (requires compiling with -DVIZ_MESH\n");
      printf(" -h              : This message\n");
      printf(" -cp <interval>  : Checkpoint frequency (0 disables checkpoints)\n");
      printf(" -ckpt <path>    : AURORA checkpoint directory or VELOC config file\n");
      printf(" -ckpt-host <h>  : AURORA backend hostname (optional)\n");
      printf("\n\n");
   }
}

static void ParseError(const char *message, int myRank)
{
   if (myRank == 0) {
      printf("%s\n", message);
#if USE_MPI      
      MPI_Abort(MPI_COMM_WORLD, -1);
#else
      exit(-1);
#endif
   }
}

void ParseCommandLineOptions(int argc, char *argv[],
                             int myRank, struct cmdLineOpts *opts)
{
   if(argc > 1) {
      int i = 1;

      while(i < argc) {
         int ok;
         /* -i <iterations> */
         if(strcmp(argv[i], "-i") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing integer argument to -i", myRank);
            }
            ok = StrToInt(argv[i+1], &(opts->its));
            if(!ok) {
               ParseError("Parse Error on option -i integer value required after argument\n", myRank);
            }
            i+=2;
         }
         /* -s <size, sidelength> */
         else if(strcmp(argv[i], "-s") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing integer argument to -s\n", myRank);
            }
            ok = StrToInt(argv[i+1], &(opts->nx));
            if(!ok) {
               ParseError("Parse Error on option -s integer value required after argument\n", myRank);
            }
            i+=2;
         }
         /* -r <numregions> */
         else if (strcmp(argv[i], "-r") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing integer argument to -r\n", myRank);
            }
            ok = StrToInt(argv[i+1], &(opts->numReg));
            if (!ok) {
               ParseError("Parse Error on option -r integer value required after argument\n", myRank);
            }
            i+=2;
         }
         /* -f <numfilepieces> */
         else if (strcmp(argv[i], "-f") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing integer argument to -f\n", myRank);
            }
            ok = StrToInt(argv[i+1], &(opts->numFiles));
            if (!ok) {
               ParseError("Parse Error on option -f integer value required after argument\n", myRank);
            }
            i+=2;
         }
         /* -p */
         else if (strcmp(argv[i], "-p") == 0) {
            opts->showProg = 1;
            i++;
         }
         /* -q */
         else if (strcmp(argv[i], "-q") == 0) {
            opts->quiet = 1;
            i++;
         }
         else if (strcmp(argv[i], "-b") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing integer argument to -b\n", myRank);
            }
            ok = StrToInt(argv[i+1], &(opts->balance));
            if (!ok) {
               ParseError("Parse Error on option -b integer value required after argument\n", myRank);
            }
            i+=2;
         }
         else if (strcmp(argv[i], "-c") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing integer argument to -c\n", myRank);
            }
            ok = StrToInt(argv[i+1], &(opts->cost));
            if (!ok) {
               ParseError("Parse Error on option -c integer value required after argument\n", myRank);
            }
            i+=2;
         }
         /* -v */
         else if (strcmp(argv[i], "-v") == 0) {
#if VIZ_MESH            
            opts->viz = 1;
#else
            ParseError("Use of -v requires compiling with -DVIZ_MESH\n", myRank);
#endif
            i++;
         }
         /* -h */
         else if (strcmp(argv[i], "-h") == 0) {
            PrintCommandLineOptions(argv[0], myRank);
#if USE_MPI            
            MPI_Abort(MPI_COMM_WORLD, 0);
#else
            exit(0);
#endif
         }
         else if(strcmp(argv[i], "-cp") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing integer argument to -cp", myRank);
            }
            ok = StrToInt(argv[i+1], &(opts->cpInterval));
            if(!ok) {
               ParseError("Parse Error on option -cp integer value required after argument\n", myRank);
            }
            i+=2;
         }
         else if(strcmp(argv[i], "-ckpt") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing argument to -ckpt\n", myRank);
            }
            opts->ckptPath = argv[i+1];
            i+=2;
         }
         else if(strcmp(argv[i], "-ckpt-host") == 0) {
            if (i+1 >= argc) {
               ParseError("Missing argument to -ckpt-host\n", myRank);
            }
            opts->ckptHost = argv[i+1];
            i+=2;
         }
         else {
            char msg[80];
            PrintCommandLineOptions(argv[0], myRank);
            sprintf(msg, "ERROR: Unknown command line argument: %s\n", argv[i]);
            ParseError(msg, myRank);
         }
      }
   }
}

/////////////////////////////////////////////////////////////////////

void VerifyAndWriteFinalOutput(Real_t elapsed_time,
                               Domain& locDom,
                               Int_t nx,
                               Int_t numRanks)
{
   // GrindTime1 only takes a single domain into account, and is thus a good way to measure
   // processor speed indepdendent of MPI parallelism.
   // GrindTime2 takes into account speedups from MPI parallelism 
   Real_t grindTime1 = ((elapsed_time*1e6)/locDom.cycle())/(nx*nx*nx);
   Real_t grindTime2 = ((elapsed_time*1e6)/locDom.cycle())/(nx*nx*nx*numRanks);

   Index_t ElemId = 0;
   printf("Run completed:  \n");
   printf("   Problem size        =  %i \n",    nx);
   printf("   MPI tasks           =  %i \n",    numRanks);
   printf("   Iteration count     =  %i \n",    locDom.cycle());
   printf("   Final Origin Energy = %12.6e \n", locDom.e(ElemId));

   Real_t   MaxAbsDiff = Real_t(0.0);
   Real_t TotalAbsDiff = Real_t(0.0);
   Real_t   MaxRelDiff = Real_t(0.0);

   for (Index_t j=0; j<nx; ++j) {
      for (Index_t k=j+1; k<nx; ++k) {
         Real_t AbsDiff = FABS(locDom.e(j*nx+k)-locDom.e(k*nx+j));
         TotalAbsDiff  += AbsDiff;

         if (MaxAbsDiff <AbsDiff) MaxAbsDiff = AbsDiff;

         Real_t RelDiff = AbsDiff / locDom.e(k*nx+j);

         if (MaxRelDiff <RelDiff)  MaxRelDiff = RelDiff;
      }
   }

   // Quick symmetry check
   printf("   Testing Plane 0 of Energy Array on rank 0:\n");
   printf("        MaxAbsDiff   = %12.6e\n",   MaxAbsDiff   );
   printf("        TotalAbsDiff = %12.6e\n",   TotalAbsDiff );
   printf("        MaxRelDiff   = %12.6e\n\n", MaxRelDiff   );

   // Timing information
   printf("\nElapsed time         = %10.2f (s)\n", elapsed_time);
   printf("Grind time (us/z/c)  = %10.8g (per dom)  (%10.8g overall)\n", grindTime1, grindTime2);
   printf("FOM                  = %10.8g (z/s)\n\n", 1000.0/grindTime2); // zones per second

   return ;
}

/////////////////////////////////////////////////////////
// Protect application states for checkpointing

void ProtectLULESH(Domain& locDom) {
   /* Node-centered state */
   LULESH_CKPT_PROTECT_REGION(0, locDom.m_x.data(), locDom.m_x.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(1, locDom.m_y.data(), locDom.m_y.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(2, locDom.m_z.data(), locDom.m_z.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(3, locDom.m_xd.data(), locDom.m_xd.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(4, locDom.m_yd.data(), locDom.m_yd.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(5, locDom.m_zd.data(), locDom.m_zd.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(6, locDom.m_xdd.data(), locDom.m_xdd.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(7, locDom.m_ydd.data(), locDom.m_ydd.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(8, locDom.m_zdd.data(), locDom.m_zdd.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(9, locDom.m_fx.data(), locDom.m_fx.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(10, locDom.m_fy.data(), locDom.m_fy.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(11, locDom.m_fz.data(), locDom.m_fz.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(12, locDom.m_nodalMass.data(),
                              locDom.m_nodalMass.size(), sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(13, locDom.m_symmX.data(), locDom.m_symmX.size(),
                              sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(14, locDom.m_symmY.data(), locDom.m_symmY.size(),
                              sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(15, locDom.m_symmZ.data(), locDom.m_symmZ.size(),
                              sizeof(Index_t));
   /* Region state */
   LULESH_CKPT_PROTECT_REGION(16, &locDom.m_numReg, 1, sizeof(Int_t));
   LULESH_CKPT_PROTECT_REGION(17, &locDom.m_cost, 1, sizeof(Int_t));
   LULESH_CKPT_PROTECT_REGION(18, locDom.m_regElemSize, locDom.m_numReg,
                              sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(19, &locDom.m_numElem, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(20, locDom.m_regNumList, locDom.m_numElem,
                              sizeof(Index_t));
   /* Element-centered state */
   LULESH_CKPT_PROTECT_REGION(21, locDom.m_nodelist.data(),
                              locDom.m_nodelist.size(), sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(22, locDom.m_lxim.data(), locDom.m_lxim.size(),
                              sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(23, locDom.m_lxip.data(), locDom.m_lxip.size(),
                              sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(24, locDom.m_letam.data(), locDom.m_letam.size(),
                              sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(25, locDom.m_letap.data(), locDom.m_letap.size(),
                              sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(26, locDom.m_lzetam.data(),
                              locDom.m_lzetam.size(), sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(27, locDom.m_lzetap.data(),
                              locDom.m_lzetap.size(), sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(28, locDom.m_elemBC.data(),
                              locDom.m_elemBC.size(), sizeof(Int_t));
   /* Temporary gradient/state arrays */
   LULESH_CKPT_PROTECT_REGION(29, locDom.m_dxx.data(), locDom.m_dxx.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(30, locDom.m_dyy.data(), locDom.m_dyy.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(31, locDom.m_dzz.data(), locDom.m_dzz.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(32, locDom.m_delv_xi.data(),
                              locDom.m_delv_xi.size(), sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(33, locDom.m_delv_eta.data(),
                              locDom.m_delv_eta.size(), sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(34, locDom.m_delv_zeta.data(),
                              locDom.m_delv_zeta.size(), sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(35, locDom.m_delx_xi.data(),
                              locDom.m_delx_xi.size(), sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(36, locDom.m_delx_eta.data(),
                              locDom.m_delx_eta.size(), sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(37, locDom.m_delx_zeta.data(),
                              locDom.m_delx_zeta.size(), sizeof(Real_t));
   /* Element state */
   LULESH_CKPT_PROTECT_REGION(38, locDom.m_e.data(), locDom.m_e.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(39, locDom.m_p.data(), locDom.m_p.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(40, locDom.m_q.data(), locDom.m_q.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(41, locDom.m_ql.data(), locDom.m_ql.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(42, locDom.m_qq.data(), locDom.m_qq.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(43, locDom.m_v.data(), locDom.m_v.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(44, locDom.m_volo.data(), locDom.m_volo.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(45, locDom.m_vnew.data(), locDom.m_vnew.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(46, locDom.m_delv.data(), locDom.m_delv.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(47, locDom.m_vdov.data(), locDom.m_vdov.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(48, locDom.m_arealg.data(),
                              locDom.m_arealg.size(), sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(49, locDom.m_ss.data(), locDom.m_ss.size(),
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(50, locDom.m_elemMass.data(),
                              locDom.m_elemMass.size(), sizeof(Real_t));
   /* Timestep state */
   LULESH_CKPT_PROTECT_REGION(51, &locDom.m_dtcourant, 1, sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(52, &locDom.m_dthydro, 1, sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(53, &locDom.m_cycle, 1, sizeof(Int_t));
   LULESH_CKPT_PROTECT_REGION(54, &locDom.m_dtfixed, 1, sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(55, &locDom.m_time, 1, sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(56, &locDom.m_deltatime, 1, sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(57, &locDom.m_deltatimemultlb, 1, sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(58, &locDom.m_deltatimemultub, 1, sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(59, &locDom.m_dtmax, 1, sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(60, &locDom.m_stoptime, 1, sizeof(Real_t));
   /* Domain decomposition state */
   LULESH_CKPT_PROTECT_REGION(61, &locDom.m_numRanks, 1, sizeof(Int_t));
   LULESH_CKPT_PROTECT_REGION(62, &locDom.m_colLoc, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(63, &locDom.m_rowLoc, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(64, &locDom.m_planeLoc, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(65, &locDom.m_tp, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(66, &locDom.m_sizeX, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(67, &locDom.m_sizeY, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(68, &locDom.m_sizeZ, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(69, &locDom.m_numElem, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(70, &locDom.m_numNode, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(71, &locDom.m_maxPlaneSize, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(72, &locDom.m_maxEdgeSize, 1, sizeof(Index_t));
   /* Node-to-element connectivity (only allocated for threaded builds) */
   LULESH_CKPT_PROTECT_REGION(73, locDom.m_nodeElemStart,
                              (locDom.m_nodeElemStart != NULL)
                                  ? locDom.numNode() + 1
                                  : 0,
                              sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(74, locDom.m_nodeElemCornerList,
                              (locDom.m_nodeElemStart != NULL)
                                  ? locDom.m_nodeElemStart[locDom.numNode()]
                                  : 0,
                              sizeof(Index_t));
   /* Boundary state */
   LULESH_CKPT_PROTECT_REGION(75, &locDom.m_rowMin, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(76, &locDom.m_rowMax, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(77, &locDom.m_colMin, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(78, &locDom.m_colMax, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(79, &locDom.m_planeMin, 1, sizeof(Index_t));
   LULESH_CKPT_PROTECT_REGION(80, &locDom.m_planeMax, 1, sizeof(Index_t));
   /* MPI communication buffers */
   LULESH_CKPT_PROTECT_REGION(81, &locDom.commBufSize, 1, sizeof(int));
   LULESH_CKPT_PROTECT_REGION(82, locDom.commDataSend, locDom.commBufSize,
                              sizeof(Real_t));
   LULESH_CKPT_PROTECT_REGION(83, locDom.commDataRecv, locDom.commBufSize,
                              sizeof(Real_t));

   for(Index_t region = 0; region < locDom.numReg(); ++region) {
      LULESH_CKPT_PROTECT_REGION(85 + region, locDom.m_regElemlist[region],
                                 locDom.regElemSize(region), sizeof(Index_t));
   }
}
