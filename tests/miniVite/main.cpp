// ***********************************************************************
//
//                              miniVite
//
// ***********************************************************************
//
//       Copyright (2018) Battelle Memorial Institute
//                      All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// ************************************************************************ 


#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include <cassert>
#include <cstdlib>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

//#include <omp.h>
#include <mpi.h>

#include "dspl.hpp"

/* for sleep function */
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#ifdef TIMER
double acc_write_time=0;
#endif

static std::string inputFileName;
static int me, nprocs;
static int ranksPerNode = 1;
static GraphElem nvRGG = 0;
static bool generateGraph = false;
static bool readBalanced = false;
static int randomEdgePercent = 0;
static bool randomNumberLCG = false;
static bool isUnitEdgeWeight = true;
static GraphWeight threshold = 1.0E-6;
static char *ckptPath = NULL;
static char *ckptHost = NULL;

// parse command line parameters
static void parseCommandLine(const int argc, char * const argv[]);

int main(int argc, char *argv[])
{
#ifdef TIMER
   double elapsed_time;
   struct timeval start;
   struct timeval end;
#endif

  int max_threads=0;

  //max_threads = omp_get_max_threads();

  if (max_threads > 1) {
      int provided;
      MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
      if (provided < MPI_THREAD_FUNNELED) {
          std::cerr << "MPI library does not support MPI_THREAD_FUNNELED." << std::endl;
          MPI_Abort(MPI_COMM_WORLD, -99);
      }
  } else {
      MPI_Init(&argc, &argv);
  }
#ifdef TIMER
   gettimeofday(&start, NULL) ;
#endif

#ifdef TIMER
   struct timeval tv;
   gettimeofday( &tv, NULL );
   double ts = tv.tv_sec + tv.tv_usec / 1000000.0;
   char hostname[64];
   gethostname(hostname, 64);
   printf("TIMESTAMP START/RESTART: %lf (s) node %s daemon %d\n", ts, hostname, getpid());
   fflush(stdout);
#endif

  double t0, t1, t2, t3, ti = 0.0;

  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &me);

  parseCommandLine(argc, argv);
  MINIVITE_CKPT_INIT(ckptPath, ckptHost);

   //char hostname[65];
   gethostname(hostname, 64);
   printf("%s daemon %d rank %d\n", hostname, (int) getpid(), me);
   //sleep(45);

  createCommunityMPIType();
  double td0, td1, td, tdt;

  MPI_Barrier(MPI_COMM_WORLD);
  td0 = MPI_Wtime();

  Graph* g = nullptr;

  // generate graph only supports RGG as of now
  if (generateGraph) { 
      GenerateRGG gr(nvRGG);
      g = gr.generate(randomNumberLCG, isUnitEdgeWeight, randomEdgePercent);
      //g->print(false);
  }
  else { // read input graph
      BinaryEdgeList rm;
      if (readBalanced == true)
          g = rm.read_balanced(me, nprocs, ranksPerNode, inputFileName);
      else
          g = rm.read(me, nprocs, ranksPerNode, inputFileName);
      //g->print();
  }

  int recovered = 0;

  assert(g != nullptr);
#ifdef PRINT_DIST_STATS 
  g->print_dist_stats();
#endif

  MPI_Barrier(MPI_COMM_WORLD);
#ifdef DEBUG_PRINTF  
  assert(g);
#endif  
  td1 = MPI_Wtime();
  td = td1 - td0;

  MPI_Reduce(&td, &tdt, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
 
  if (me == 0)  {
      if (!generateGraph)
          std::cout << "Time to read input file and create distributed graph (in s): " 
              << (tdt/nprocs) << std::endl;
      else
          std::cout << "Time to generate distributed graph of " 
              << nvRGG << " vertices (in s): " << (tdt/nprocs) << std::endl;
  }

  GraphWeight currMod = -1.0;
  GraphWeight prevMod = -1.0;
  double total = 0.0;

  std::vector<GraphElem> ssizes, rsizes, svdata, rvdata;
#if defined(USE_MPI_RMA)
  MPI_Win commwin;
#endif
  size_t ssz = 0, rsz = 0;
  int iters = 0;
    
  MPI_Barrier(MPI_COMM_WORLD);

  t1 = MPI_Wtime();

#if defined(USE_MPI_RMA)
  currMod = distLouvainMethod(me, nprocs, *g, ssz, rsz, ssizes, rsizes, 
                svdata, rvdata, currMod, threshold, iters, commwin);
#else
  currMod = distLouvainMethod(me, nprocs, *g, ssz, rsz, ssizes, rsizes, 
                svdata, rvdata, currMod, threshold, iters);
#endif
  MPI_Barrier(MPI_COMM_WORLD);
  t0 = MPI_Wtime();
  
  if(me == 0) {
      std::cout << "Modularity: " << currMod << ", Iterations: " 
          << iters << ", Time (in s): "<<t0-t1<< std::endl;

      std::cout << "**********************************************************************" << std::endl;
  }

  MPI_Barrier(MPI_COMM_WORLD);

  double tot_time = 0.0;
  MPI_Reduce(&total, &tot_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  
  delete g;
  destroyCommunityMPIType();

MINIVITE_CKPT_FINALIZE();

#ifdef TIMER
   gettimeofday(&end, NULL) ;
   elapsed_time = (double)(end.tv_sec - start.tv_sec) + ((double)(end.tv_usec - start.tv_usec))/1000000 ;
   //char hostname[64];
   gethostname(hostname, 64);
   printf("APP EXE TIME: %lf (s) node %s daemon %d \n", elapsed_time, hostname, getpid());
   fflush(stdout);
#endif

   printf("WRITE CP TIME: %lf (s) node %s daemon %d \n", acc_write_time, hostname, getpid());
   fflush(stdout);

// close MPI
  MPI_Finalize();

  return 0;
}

static void printUsage(FILE *out)
{
  fprintf(out,
    "Usage: miniVite [options]\n"
    "\n"
    "Input (exactly one of):\n"
    "  -f <bin-file>       read a binary graph file\n"
    "  -n <vertices>       generate a random geometric graph with <vertices> vertices\n"
    "\n"
    "Graph options:\n"
    "  -b                  balance edges across ranks (-f only)\n"
    "  -l                  use the distributed LCG for random numbers (-n only)\n"
    "  -p <percent>        percent of edges placed randomly between processes (-n only, default 0)\n"
    "  -w                  use Euclidean distance as edge weight (-n only)\n"
    "  -r <ranks>          ranks per node (default 1)\n"
    "  -t <threshold>      Louvain convergence threshold (default 1.0E-06)\n"
    "\n"
#if defined(MINIVITE_CKPT_AURORA) || defined(MINIVITE_CKPT_VELOC)
    "Checkpoint options (required by this build):\n"
    "  -ckpt <path>        AURORA checkpoint directory or VELOC config file\n"
    "  -ckpt-host <host>   AURORA backend hostname (optional)\n"
    "\n"
#endif
    "  -h, --help          show this message\n");
}

static void usageError(const std::string &message)
{
  if (me == 0) {
    if (!message.empty())
      std::cerr << "miniVite: " << message << std::endl;
    printUsage(stderr);
  }
  MPI_Finalize();
  exit(1);
}

void parseCommandLine(const int argc, char * const argv[])
{
  int ret;

  if (argc == 1)
      usageError("");

  int i = 1;
  while (i < argc) {
      const char *opt = argv[i];
      if (strcmp(opt, "-h") == 0 || strcmp(opt, "--help") == 0) {
          if (me == 0)
              printUsage(stdout);
          MPI_Finalize();
          exit(0);
      }

      bool flag = (strcmp(opt, "-b") == 0) || (strcmp(opt, "-l") == 0) ||
                  (strcmp(opt, "-w") == 0);
      bool valued = (strcmp(opt, "-f") == 0) || (strcmp(opt, "-n") == 0) ||
                    (strcmp(opt, "-r") == 0) || (strcmp(opt, "-t") == 0) ||
                    (strcmp(opt, "-p") == 0) ||
                    (strcmp(opt, "-ckpt") == 0) || (strcmp(opt, "-ckpt-host") == 0);
      if (!flag && !valued)
          usageError(std::string("unknown option ") + opt);
      if (valued && (i + 1 >= argc))
          usageError(std::string("missing argument to ") + opt);
      const char *val = valued ? argv[i + 1] : NULL;

      if (strcmp(opt, "-f") == 0) {
          inputFileName.assign(val);
      }
      else if (strcmp(opt, "-n") == 0) {
          nvRGG = atol(val);
          if (nvRGG <= 0)
              usageError("-n requires a positive number of vertices");
          generateGraph = true;
      }
      else if (strcmp(opt, "-b") == 0) {
          readBalanced = true;
      }
      else if (strcmp(opt, "-r") == 0) {
          ranksPerNode = atoi(val);
      }
      else if (strcmp(opt, "-t") == 0) {
          threshold = atof(val);
      }
      else if (strcmp(opt, "-w") == 0) {
          isUnitEdgeWeight = false;
      }
      else if (strcmp(opt, "-l") == 0) {
          randomNumberLCG = true;
      }
      else if (strcmp(opt, "-p") == 0) {
          randomEdgePercent = atoi(val);
      }
      else if (strcmp(opt, "-ckpt") == 0) {
          ckptPath = argv[i + 1];
      }
      else if (strcmp(opt, "-ckpt-host") == 0) {
          ckptHost = argv[i + 1];
      }

      i += valued ? 2 : 1;
  }

/*
  while ((ret = getopt(argc, argv, "f:br:t:n:wlp:")) != -1) {
    switch (ret) {
    case 'f':
      inputFileName.assign(optarg);
      break;
    case 'b':
      readBalanced = true;
      break;
    case 'r':
      ranksPerNode = atoi(optarg);
      break;
    case 't':
      threshold = atof(optarg);
      break;
    case 'n':
      nvRGG = atol(optarg);
      if (nvRGG > 0)
          generateGraph = true; 
      break;
    case 'w':
      isUnitEdgeWeight = false;
      break;
    case 'l':
      randomNumberLCG = true;
      break;
    case 'p':
      randomEdgePercent = atoi(optarg);
      break;
    default:
      //assert(0 && "Should not reach here!!");
      break;
    }
  }
*/

  if (generateGraph && !inputFileName.empty())
      usageError("-f and -n cannot be used together");

  if (!generateGraph && inputFileName.empty())
      usageError("specify an input with -f <bin-file> or -n <vertices>");

  if (!generateGraph && randomNumberLCG)
      usageError("-l requires -n");

  if (!generateGraph && randomEdgePercent)
      usageError("-p requires -n");

  if (!generateGraph && !isUnitEdgeWeight)
      usageError("-w requires -n");

  if (generateGraph && ((randomEdgePercent < 0) || (randomEdgePercent >= 100)))
      usageError("-p must be between 0 and 99");
} // parseCommandLine

