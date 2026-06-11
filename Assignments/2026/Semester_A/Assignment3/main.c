#include "guassonFilter.h"
#include <stdlib.h>
#include <stdio.h>

int main() {
    const char *inputFilename = "input_image.png";
    const char *outputFilename = "output_image.png";
    int blurRadius = 5;

    Image *inputImage = loadImage(inputFilename);
    if (inputImage == NULL) {
        printf("Failed to load image\n");
        return -1;
    }

    Image *outputImage = createBlurredImage(blurRadius, inputImage);
    saveImage(outputFilename, outputImage);

    free(inputImage->pixels);
    free(inputImage);
    free(outputImage->pixels);
    free(outputImage);

    return 0;
}
