//
//  guassonFilter.h
//  guasonFilter
//
//  Created by Saar Azari on 04/07/2024.
//

#ifndef guassonFilter_h
#define guassonFilter_h

// Define an RGBA pixel structure
typedef struct {
    unsigned char r, g, b, a;
} RGBA;

// Define an Image structure
typedef struct {
    int width;
    int height;
    RGBA *pixels;
} Image;

Image *loadImage(const char *filename);

void saveImage(const char *filename, Image *image);

void createGaussianKernel(int radius, double sigma, double **kernel, double *sum);

Image *createBlurredImage(int radius, Image *image);

#endif /* guassonFilter_h */
