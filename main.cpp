// This program is free software: you can use, modify and/or redistribute it
// under the terms of the simplified BSD License. You should have received a
// copy of this license along this program. If not, see
// <http://www.opensource.org/licenses/bsd-license.html>.
//
// Copyright (C) 2018, Thibaud Briand <thibaud.briand@enpc.fr>
// Copyright (C) 2015, Javier Sánchez Pérez <jsanchez@dis.ulpgc.es>
// All rights reserved.

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <algorithm>
#include "inverse_compositional_algorithm.h"
#include "file.h"
#include "transformation.h"
#include "bicubic_interpolation.h"
#include "smapa.h"

#define PAR_DEFAULT_NSCALES 0
#define PAR_DEFAULT_ZFACTOR 0.5
#define PAR_DEFAULT_TOL 0.001
#define PAR_DEFAULT_TYPE 8
#define PAR_DEFAULT_ROBUST 3
#define PAR_DEFAULT_LAMBDA 0.0
#define PAR_DEFAULT_VERBOSE 0
#define PAR_DEFAULT_OUTFILE "transform.mat"
#define PAR_DEFAULT_FIRST_SCALE 0
#define PAR_DEFAULT_GRAYMETHOD 1
#define PAR_DEFAULT_DELTA 5
#define PAR_DEFAULT_NANIFOUTSIDE 1
#define PAR_DEFAULT_TYPE_GRADIENT 3
#define PAR_DEFAULT_OUTPUT 0

SMART_PARAMETER(P0, 0.);
SMART_PARAMETER(P1, 0.);
SMART_PARAMETER(P2, 0.);
SMART_PARAMETER(P3, 0.);
SMART_PARAMETER(P4, 0.);
SMART_PARAMETER(P5, 0.);
SMART_PARAMETER(P6, 0.);
SMART_PARAMETER(P7, 0.);

/**
 *
 *  Print a help message
 *
 */
void print_help(char *name)
{
  printf("\n<Usage>: %s image1 image2 [OPTIONS] \n\n", name);
  printf("This program calculates the transformation between two images.\n");
  printf("It implements the inverse compositional algorithm. \n");
  printf("More information in http://www.ipol.im \n\n");
  printf("OPTIONS:\n");
  printf("--------\n");
  printf(" -f name \t Name of the output filename that will contain the\n");
  printf("         \t   computed transformation\n");
  printf("         \t   Default value %s\n", PAR_DEFAULT_OUTFILE);
  printf(" -o N    \t Output transformation format: \n");
  printf("         \t   0-Parametrization\n");
  printf("         \t   1-3x3 Projective matrix\n");
  printf("         \t   2-image\n");
  printf("         \t   Default value %d\n", PAR_DEFAULT_OUTPUT);
  printf(" -n N    \t Number of scales for the coarse-to-fine scheme\n");
  printf("         \t   Default value %d\n", PAR_DEFAULT_NSCALES);
  printf(" -z F    \t Zoom factor used in the coarse-to-fine scheme\n");
  printf("         \t   Values must be in the range (0,1)\n");
  printf("         \t   Default value %0.2f\n",
                        PAR_DEFAULT_ZFACTOR);
  printf(" -e F    \t Threshold for the convergence criterion \n");
  printf("         \t   Default value %0.4f\n", PAR_DEFAULT_TOL);
  printf(" -t N    \t Transformation type to be computed:\n");
  printf("         \t   2-translation; 3-Euclidean transform; 4-similarity\n");
  printf("         \t   6-affinity; 8-homography\n");
  printf("         \t   Default value %d\n",
                        PAR_DEFAULT_TYPE);
  printf(" -r N    \t Use robust error functions: \n");
  printf("         \t   0-Non robust (L2 norm); 1-truncated quadratic\n");
  printf("         \t   2-Geman & McLure; 3-Lorentzian 4-Charbonnier\n");
  printf("         \t   Default value %d\n",
                        PAR_DEFAULT_ROBUST);
  printf(" -l F    \t Value of the parameter for the robust error function\n");
  printf("         \t   A value <=0 if it is automatically computed\n");
  printf("         \t   Default value %0.0f\n", PAR_DEFAULT_LAMBDA);
  printf(" -s N    \t First scale used in the pyramid\n");
  printf("         \t   Default value %d\n", PAR_DEFAULT_FIRST_SCALE);
  printf(" -c N    \t Use grayscale conversion (1) or not (0)\n");
  printf("         \t   Default value %d\n",
                        PAR_DEFAULT_GRAYMETHOD);
  printf(" -d N    \t Distance to the boundary\n");
  printf("         \t   Default value %d\n",
                        PAR_DEFAULT_DELTA);
  printf(" -p N    \t Parameter to discards boudary pixels (1) or not (0)\n");
  printf("         \t   Default value %d\n",
                        PAR_DEFAULT_NANIFOUTSIDE);
  printf(" -g N    \t Use gradient type: \n");
  printf("         \t   0-Central differences; 1-Hypomode\n");
  printf("         \t   2-Farid 3x3; 3-Farid 5x5; 4-Sigma 3; 5-Sigma 6\n");
  printf("         \t   Default value %d\n",
                        PAR_DEFAULT_TYPE_GRADIENT);
  printf(" -L 1    \t Apply the Laplacian at each scale.\n");
  printf(" -v      \t Switch on verbose mode. \n\n\n");
}

/**
 *
 *  Read command line parameters
 *
 */
int read_parameters(
    int    argc,
    char   *argv[],
    char   **image1,
    char   **image2,
    char   const **outfile,
    int    &nscales,
    double &zfactor,
    double &TOL,
    int    &nparams,
    int    &robust,
    double &lambda,
    int    &laplacian,
    int    &verbose,
    int    &first_scale,
    int    &graymethod,
    int    &delta,
    int    &nanifoutside,
    int    &type_gradient,
    int    &type_output
)
{
  if (argc < 3){
    print_help(argv[0]);
    return 0;
  }
  else{
    int i=1;
    *image1=strdup(argv[i++]);
    *image2=strdup(argv[i++]);

    //assign default values to the parameters
    *outfile     =PAR_DEFAULT_OUTFILE;
    nscales      =PAR_DEFAULT_NSCALES;
    zfactor      =PAR_DEFAULT_ZFACTOR;
    TOL          =PAR_DEFAULT_TOL;
    nparams      =PAR_DEFAULT_TYPE;
    robust       =PAR_DEFAULT_ROBUST;
    lambda       =PAR_DEFAULT_LAMBDA;
    laplacian    =0;
    verbose      =PAR_DEFAULT_VERBOSE;
    first_scale  =PAR_DEFAULT_FIRST_SCALE;
    graymethod   =PAR_DEFAULT_GRAYMETHOD;
    delta        =PAR_DEFAULT_DELTA;
    nanifoutside =PAR_DEFAULT_NANIFOUTSIDE;
    type_gradient=PAR_DEFAULT_TYPE_GRADIENT;
    type_output  =PAR_DEFAULT_OUTPUT;

    //read each parameter from the command line
    while(i<argc)
    {
      if(strcmp(argv[i],"-f")==0)
        if(i<argc-1)
          *outfile = argv[++i];

      if(strcmp(argv[i],"-n")==0)
        if(i<argc-1)
          nscales=atoi(argv[++i]);

      if(strcmp(argv[i],"-z")==0)
        if(i<argc-1)
          zfactor=atof(argv[++i]);

      if(strcmp(argv[i],"-e")==0)
        if(i<argc-1)
          TOL=atof(argv[++i]);

      if(strcmp(argv[i],"-t")==0)
        if(i<argc-1)
          nparams=atoi(argv[++i]);

      if(strcmp(argv[i],"-r")==0)
        if(i<argc-1)
          robust=atoi(argv[++i]);

      if(strcmp(argv[i],"-l")==0)
        if(i<argc-1)
          lambda=atof(argv[++i]);

      if(strcmp(argv[i],"-s")==0)
        if(i<argc-1)
          first_scale=atoi(argv[++i]);

      if(strcmp(argv[i],"-c")==0)
        if(i<argc-1)
          graymethod=atoi(argv[++i]);

      if(strcmp(argv[i],"-d")==0)
        if(i<argc-1)
          delta=atoi(argv[++i]);

      if(strcmp(argv[i],"-p")==0)
        if(i<argc-1)
          nanifoutside=atoi(argv[++i]);

      if(strcmp(argv[i],"-g")==0)
        if(i<argc-1)
          type_gradient=atoi(argv[++i]);

      if(strcmp(argv[i],"-o")==0)
        if(i<argc-1)
          type_output=atoi(argv[++i]);

      if(strcmp(argv[i],"-L")==0)
        laplacian=atoi(argv[++i]);

      if(strcmp(argv[i],"-v")==0)
        verbose=1;

      i++;
    }

    //check parameter values
    if(zfactor<=0||zfactor>=1)
        zfactor=PAR_DEFAULT_ZFACTOR;
    if(TOL<0)
        TOL=PAR_DEFAULT_TOL;
    if(nparams!=2 && nparams!=3 && nparams!=4 && nparams!=6 && nparams!=8)
        nparams=PAR_DEFAULT_TYPE;
    if(robust<0||robust>4)
        robust=PAR_DEFAULT_ROBUST;
    if(lambda<0)
        lambda=PAR_DEFAULT_LAMBDA;
    if(delta<0)
        delta=PAR_DEFAULT_DELTA;
    if(nanifoutside != 0 && nanifoutside != 1)
        nanifoutside=PAR_DEFAULT_NANIFOUTSIDE;
    if(graymethod != 0 && graymethod != 1)
        graymethod=PAR_DEFAULT_GRAYMETHOD;
    if(type_gradient<0 || type_gradient>5)
        type_gradient=PAR_DEFAULT_TYPE_GRADIENT;
    if(type_output<0 || type_output>2)
        type_output  =PAR_DEFAULT_OUTPUT;
  }

  return 1;
}

/**
  *
  *  Function to convert an rgb image to grayscale levels
  *
**/
void rgb2gray(
  double *rgb,  //input color image
  double *gray, //output grayscale image
  int nx,       //number of pixels
  int ny,
  int nz
)
{
  int size=nx*ny;
  if( nz==3 )
    for(int i=0;i<size;i++)
      gray[i]=0.3333333333333*(rgb[i*nz]+rgb[i*nz+1]+rgb[i*nz+2]);
  else
    for(int i=0;i<size;i++)
      gray[i]=rgb[i];
}

/**
 *
 *  Main program:
 *   This program reads the following parameters from the console and
 *   computes the corresponding parametric transformation:
 *   -I1            first image
 *   -I2            second image
 *   -outfile       name of the output flow field
 *   -nscales       number of scales for the pyramidal approach
 *   -zoom_factor   reduction factor for creating the scales
 *   -TOL           stopping criterion threshold for the iterative process
 *   -type          type of the parametric model (the number of parameters):
 *                  Translation(2), Euclidean(3), Similarity(4), Affinity(6),
 *                  Homography(8)
 *   -robust        type of the robust error function
 *   -lambda        parameter of the robust error function
 *   -first_scale   first scale used in the pyramid
 *   -graymethod    parameter for grayscale conversion
 *   -delta         distance to the boundary
 *   -nanifoutside  parameter for discarding boundary pixels
 *   -type_gradient type of gradient:
 *                  0-Central differences; 1-Hypomode
 *                  2-Farid 3x3; 3-Farid 5x5; 4-Sigma 3; 5-Sigma 6
 *   -verbose       switch on/off messages
 *
 */
int main (int argc, char *argv[])
{
  //parameters of the method
  char  *image1, *image2;
  const char *outfile;
  int    nscales, nparams, robust, laplacian, verbose, first_scale, graymethod;
  int    delta, nanifoutside, type_gradient, type_output;
  double zfactor, TOL, lambda;

  //read the parameters from the console
  int result=read_parameters(
        argc, argv, &image1, &image2, &outfile, nscales,
        zfactor, TOL, nparams, robust, lambda, laplacian, verbose, first_scale,
        graymethod, delta, nanifoutside, type_gradient, type_output
      );

  if(result)
  {
    int nx, ny, nz, nx1, ny1, nz1;

    double *I1, *I2;

    char* maskimage1 = strstr(image1, ":");
    char* maskimage2 = strstr(image2, ":");
    if (maskimage1) {
      *maskimage1 = 0;
      maskimage1++;
    }
    if (maskimage2) {
      *maskimage2 = 0;
      maskimage2++;
    }

    //read the input images
    bool correct1=read_image(image1, &I1, nx, ny, nz);
    bool correct2=read_image(image2, &I2, nx1, ny1, nz1);

    double *M1 = nullptr, *M2 = nullptr;
    if (maskimage1) {
      correct1 &= read_image(maskimage1, &M1, nx, ny, nz);
      printf("use mask for image1\n");
    }
    if (maskimage2) {
      correct2 &= read_image(maskimage2, &M2, nx, ny, nz);
      printf("use mask for image2\n");
    }

    // if the images are correct, compute the estimated motion
    if (correct1 && correct2 && nx == nx1 && ny == ny1 && nz == nz1)
    {
      //limit the number of scales according to image size (min 32x32)
      int limit = 32;
      int N=1+log(std::min(nx, ny)/(float)limit)/log(1./zfactor);
      if (N<nscales || nscales <= 0) nscales= N;

      if(verbose)
        printf(
          "\nParameters: scales=%d, zoom=%f, TOL=%f, transform type=%d, "
          "robust function=%d, lambda=%f, output file=%s, delta=%d, "
          "nanifoutside=%d, graymethod=%d, first scale=%d, gradient type=%d, "
          "type output=%d\n",
          nscales, zfactor, TOL, nparams, robust, lambda, outfile, delta,
          nanifoutside, graymethod, first_scale, type_gradient, type_output
        );

      //allocate memory for the parametric model
      double *p=new double[nparams];
      switch (nparams) {
        case 8:
          p[6] = P6();
          p[7] = P7();
          // fall through
        case 6:
          p[4] = P4();
          p[5] = P5();
          // fall through
        case 4:
          p[3] = P3();
          // fall through
        case 3:
          p[2] = P2();
          // fall through
        case 2:
          p[0] = P0();
          p[1] = P1();
          // fall through
      }

      if( graymethod && nz==3 ) {
        //convert images to grayscale
        double *I1g=new double[nx*ny];
        double *I2g=new double[nx*ny];

        rgb2gray(I1, I1g, nx, ny, nz);
        rgb2gray(I2, I2g, nx, ny, nz);

        double* M1g = nullptr;
        if (M1) {
          M1g=new double[nx*ny];
          rgb2gray(M1, M1g, nx, ny, nz);
        }
        double* M2g = nullptr;
        if (M2) {
          M2g=new double[nx*ny];
          rgb2gray(M2, M2g, nx, ny, nz);
        }

        //compute the optic flow
        const clock_t begin = clock();
        pyramidal_inverse_compositional_algorithm(
           I1g, I2g, M1g, M2g, p, nparams, nx, ny, 1,
           nscales, zfactor, TOL, robust, lambda, first_scale, nanifoutside,
           delta, type_gradient, laplacian, verbose
        );

        if(verbose)
           printf("Time=%f\n", double(clock()-begin)/CLOCKS_PER_SEC);

        //free memory
        delete[]I1g;
        delete[]I2g;
        delete[]M1g;
        delete[]M2g;
      }
      else {
        //compute the optic flow
        const clock_t begin = clock();
        pyramidal_inverse_compositional_algorithm(
           I1, I2, M1, M2, p, nparams, nx, ny, nz,
           nscales, zfactor, TOL, robust, lambda, first_scale, nanifoutside,
           delta, type_gradient, laplacian, verbose
        );

        if(verbose)
           printf("Time=%f\n", double(clock()-begin)/CLOCKS_PER_SEC);
      }

      //save the parametric model to disk
      if(type_output == 1) {
        double mat[9];
        params2matrix(p, mat, nparams);
        save_matrix(outfile, mat, 9);
      } else if (type_output == 0) {
        save(outfile, p, nparams);
      } else if (type_output == 2) {
        double *Iw=new double[nx*ny*nz];
        bicubic_interpolation(I2, Iw, p, nparams, nx, ny, nz, 0, 1);
        save_image(outfile, Iw, nx, ny, nz);
        delete[] Iw;
      }
      if(verbose) {
        double mat[9];
        params2matrix(p, mat, nparams);
        printf("Transform: ");
        for(int j=0; j<9; j++) printf("%.14lg ", mat[j]);
        printf("\n");
      }

      //free memory
      delete[]p;
      //free memory
      free (I1);
      free (I2);
      free (M1);
      free (M2);
    }
    else
    {
      printf("Cannot read the images or their sizes are not the same\n");
      exit(EXIT_FAILURE);
    }
  }
  exit(EXIT_SUCCESS);
}
