// Written by Matthew Hait for ECE-4900 Lab 2
#ifndef __UTIL_H__
#define __UTIL_H__


// Stuff for reading images
/*
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Stuff for writing images
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
*/
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <sys/time.h>
#include <stdbool.h>

#define R_PRIME  0.299	/** \brief R' constant for rec601 luma (Y') calc */
#define G_PRIME  0.587  /** \brief G' constant for rec601 luma (Y') calc */
#define B_PRIME  0.114  /** \brief B' constant for rec601 luma (Y') calc */

/** \brief  Image structure (bool Pixel Size) */
typedef struct {
    int width;	    /**< Image pixel width*/
    int height;	    /**< Image pixel height*/
    int ch;		    /**< Image color channel count*/
    size_t size;	/**< Image storage size in Raster Scan*/
    bool *image;	/**< Pointer to image data*/
} rastImageBin;

/** \brief  Image structure (uint8_t Pixel Size) */
typedef struct {
	int width;	    /**< Image pixel width*/
	int height;	    /**< Image pixel height*/
	int ch;		    /**< Image color channel count*/
	size_t size;	/**< Image storage size in Raster Scan*/
	uint8_t *image;	/**< Pointer to image data*/
} rastImage;

/** \brief  Image structure (double Pixel Size) */
typedef struct {
    int width;	    /**< Image pixel width*/
    int height;	    /**< Image pixel height*/
    int ch;		    /**< Image color channel count*/
    size_t size;	/**< Image storage size in Raster Scan*/
    double *image;	/**< Pointer to image data*/
} rastImageDouble;

/** \brief Converts color rastImage struct to new grayscale struct using Luma coding.
  * \param *colorImg	Input color image struct
  * \param *grayImg	Output gray image struct
  * \param return	1: Success, 0: Error */
int img_c_to_g(const rastImage *colorImg, rastImage *grayImg);

/** \brief Loads file into image struct. Only accepts three color channel images.
  * \param *imgStruct	rastImage struct to write to
  * \param *fName	Name of image file to load.
  * \param return	1: Data loaded, 0: Wrong file type, 2: Wrong channel number, 3: Memory Error */
int img_load(rastImage *imgStruct, const char *fName);

/** \brief Preforms 2D matrix convolution on image.
  * \param *outImg	Output image struct to write to
  * \param *inImg	Input image struct to read to
  * \param return	1: Done */
int img_conv(rastImageDouble *outImg, rastImageDouble *inImg, double *kernel,int kX, int kY);

/** \brief Frees image from heap
  * \param *imgStruct	rastImage struct to remove. */
void img_free(rastImage *imgStruct);

/** \brief Frees image from heap
  * \param *imgStruct	rastImageBin struct to remove. */
void img_free_bool(rastImageBin *imgStruct);

/** \brief Frees image (double) from heap
  * \param *imgStruct	rastImage struct to remove. */
void img_free_double(rastImageDouble *imgStruct);

/** \brief Saves image struct to *.jpg file or *.bof depending on file ouput name.
  * \param *fName	    Name of file to save
  * \param *imgStruct	rastImage struct to remove
  * \param return	    1: Success, 0: Error */
int img_save(const char *fName, rastImage *imgStruct);

/** \brief Saves image struct (double) to *.jpg file or *.bof depending on file ouput name.
  * \param *fName	    Name of file to save
  * \param *imgStruct	rastImage struct to remove
  * \param *time	    Pointer to store img conversion time
  * \param return	    1: Success, 0: Error */
int img_save_double(const char *fName, rastImageDouble *imgStruct, struct timeval *time);

/** \brief Converts rastImage struct to rastImageDouble. WARNING: Will allocate new mem and free image of old struct.
  * \param *inImg	Input image struct to read to
  * \param *outImg	Output image struct to write to
  * \param return	1: Data loaded, 0:Memory Error */
int rast_rastDouble(rastImage *inImg, rastImageDouble *outImg);

/** \brief Converts rastImageDouble struct to rastImage. WARNING: Will allocate new mem and free image of old struct.
  * \param *inImg	Input image struct to read to
  * \param *outImg	Output image struct to write to
  * \param return	1: Data loaded, 0:Memory Error */
int rastDouble_rast(rastImageDouble *inImg, rastImage *outImg);

#endif
