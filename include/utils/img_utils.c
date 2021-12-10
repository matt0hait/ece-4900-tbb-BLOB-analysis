#include <stdlib.h>
#include <stdio.h>
#include "img_utils.h"
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <sys/time.h>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Stuff for writing images
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int img_c_to_g(const rastImage *colorImg, rastImage *grayImg) {

    uint8_t *iPNT; uint8_t *oiPNT;
    for(iPNT = colorImg->image, oiPNT = grayImg->image; iPNT != colorImg->image + colorImg->size; iPNT += 3, oiPNT += 1) {
        *oiPNT = (uint8_t)((*iPNT *  R_PRIME) + (*(iPNT+1) * G_PRIME) + (*(iPNT+2) * B_PRIME));
    }
    grayImg->size = colorImg->width*colorImg->height;
    grayImg->ch = 1;
    return(1);
}

int img_load(rastImage *imgStruct, const char *fName) {
	int cw,ch,cn;
	if(!stbi_info(fName, &cw, &ch, &cn)) {
			printf("Image selected is not a supported format\n");
			return(0);
	}
	if (cn != 3 && cn != 1) {
			printf("Unsupported format. Looking for three channel (R, G , B), or grayscale.\n");
			return(2);
	}
	imgStruct->image  = stbi_load(fName, &imgStruct->width, &imgStruct->height, &imgStruct->ch, 0);
	if(imgStruct->image == NULL) {
			printf("Error loading image\n");
			return(3);
	} else {
		imgStruct->size = imgStruct->width*imgStruct->height*imgStruct->ch;
		return(1);
	}
}

int img_conv(rastImageDouble *outImg, rastImageDouble *inImg, double *kernel,int kX, int kY) {
    // See Llamocca's conv2 library for source reference.
    int sX, sY;
    double *I, *K, *O;
    I = inImg->image; O = outImg->image; sX = inImg->width; sY = inImg->height;
    K = kernel;
    int i, j, m, n, mm, nn;
    int kCX, kCY; // center index of kernel
    double sum; // temp accumulation buffer
    int i_I, j_I;
    if (!I || !O || !K) return 0;
    if (sX <= 0 || kX <= 0) return 0;
    kCX = kX / 2; kCY = kY / 2;
    for (i = 0; i < sY; ++i) { // Output matrix: rows
        for (j = 0; j < sX; ++j) { // Output matrix: columns
            sum = 0; // init to 0 before sum
            for (m = 0; m < kY; ++m) { // Kernel: rows
                mm = kY - 1 - m; // Flipped kernel: index for rows
                for (n = 0; n < kX; ++n) { // Kernel: columns
                    nn = kX - 1 - n; // Flipped kernel: index for columns
                    i_I = i + (kCY - mm);
                    j_I = j + (kCX - nn);
                    // out-of-bounds input samples are set to 0
                    if (i_I >= 0 && i_I < sY && j_I >= 0 && j_I < sX) {
                        sum += I[sX * i_I + j_I] * K[kX * mm + nn];
                    }
                }
            }
            //out[dataSizeX * i + j] = (unsigned char)((float)fabs(sum) + 0.5f);
            O[sX*i + j] = sum; // fabs work for float?
        }
    }
    return 1;
}

void img_free(rastImage *imgStruct) {
	stbi_image_free(imgStruct->image);
}

void img_free_bool(rastImageBin *imgStruct) {
    stbi_image_free(imgStruct->image);
}

void img_free_double(rastImageDouble *imgStruct) {
    stbi_image_free(imgStruct->image);
}

int img_save(const char *fName, rastImage *imgStruct) {
    int len = (int)strlen(fName);
    if (len < 5) {
        printf("ERROR: Bad file output name: \"%s\".\n",fName);
        return 0;
    }
    const char *suffix = &fName[len-4];
    if (strcmp(suffix, ".jpg") == 0 ) {
        stbi_write_jpg(fName, imgStruct->width, imgStruct->height, imgStruct->ch, imgStruct->image, 100);
    }
    if (strcmp(suffix, ".bof") == 0) {
        // Output binary file
        FILE *file_o;
        uint8_t *ixPNT; int *oixPNT;
        int *outBuff = (int *)malloc(sizeof(int)*imgStruct->size);
        if(outBuff == NULL) { printf("Ope. Unable to allocate memory for comp (img_save).\n"); return(0); }
        for(ixPNT = imgStruct->image, oixPNT = outBuff; ixPNT != imgStruct->image + imgStruct->size; ixPNT += 1, oixPNT += 1) {
            *oixPNT = (int)*ixPNT;
        }
        file_o = fopen(fName,"w+b");
        fwrite(outBuff,sizeof(uint32_t),imgStruct->size,file_o);
        if(ferror(file_o)){
            printf("Error: Unable to write output file.\n");
        }
        fclose(file_o);
        free(outBuff);
    }
    if ((strcmp(suffix, ".bof") != 0) & (strcmp(suffix, ".jpg") != 0 )) {
        printf("ERROR: Bad file output name: \"%s\".\n",fName);
        return 0;
    }
    return 1;
}

int img_save_double(const char *fName, rastImageDouble *imgStruct, struct timeval *time) {
    int len = (int)strlen(fName);
    if (len < 5) {
        printf("ERROR: Bad file output name: \"%s\".\n",fName);
        return 0;
    }
    const char *suffix = &fName[len-4];
    if (strcmp(suffix, ".jpg") == 0 ) {
        rastImage tempImg;
        if(!rastDouble_rast(imgStruct,&tempImg)) return -1;
        stbi_write_jpg(fName, tempImg.width, tempImg.height, tempImg.ch, tempImg.image, 100);
        if(!rast_rastDouble(&tempImg,imgStruct)) return -1;
    }
    if (strcmp(suffix, ".bof") == 0) {
        // Output binary file
        FILE *file_o;
        double *ixPNT; int *oixPNT;
        int *outBuff = (int *)malloc(sizeof(int)*imgStruct->size);
        if(outBuff == NULL) { printf("Ope. Unable to allocate memory for comp (img_save_double).\n"); return(0); }
        for(ixPNT = imgStruct->image, oixPNT = outBuff; ixPNT != imgStruct->image + imgStruct->size; ixPNT += 1, oixPNT += 1) {
            // if (x >= 0) xr = x + 0.5; else xr = x-0.5;
            if(ixPNT >= 0) *oixPNT = (int)(*ixPNT+0.5); else *oixPNT = (int)(*ixPNT-0.5);
            //*oixPNT = (int)*ixPNT;
        }
        gettimeofday(time,NULL);
        file_o = fopen(fName,"w+b");
        fwrite(outBuff,sizeof(uint32_t),imgStruct->size,file_o);
        if(ferror(file_o)){
            printf("Error: Unable to write output file.\n");
        }
        fclose(file_o);
        free(outBuff);
    }
    if ((strcmp(suffix, ".bof") != 0) & (strcmp(suffix, ".jpg") != 0 )) {
        printf("ERROR: Bad file output name: \"%s\".\n",suffix);
        return 0;
    }
    return 1;
}

int rast_rastDouble(rastImage *inImg, rastImageDouble *outImg) {
    uint8_t *iPNT; double *oiPNT;
    outImg->width = inImg->width;
    outImg->height = inImg->height;
    outImg->size = inImg->size;
    outImg->ch = inImg->ch;
    outImg->image = malloc((sizeof(double )/sizeof(uint8_t))*outImg->size);
    if(outImg->image == NULL) { printf("Ope. Unable to allocate memory for comp (rast_rastDouble).\n"); return(0); }
    for(iPNT = inImg->image, oiPNT = outImg->image; iPNT != inImg->image + inImg->size; iPNT += 1, oiPNT += 1) {
        *oiPNT = (double)*iPNT;
    }
    free(inImg->image);
    return(1);
}

int rastDouble_rast(rastImageDouble *inImg, rastImage *outImg) {
    double *iPNT; uint8_t *oiPNT;
    outImg->width = inImg->width;
    outImg->height = inImg->height;
    outImg->size = inImg->size;
    outImg->ch = inImg->ch;
    outImg->image = malloc((sizeof(double )/sizeof(uint8_t))*outImg->size);
    if(outImg->image == NULL) { printf("Ope. Unable to allocate memory for comp (rastDouble_rast).\n"); return(0); }
    for(iPNT = inImg->image, oiPNT = outImg->image; iPNT != inImg->image + inImg->size; iPNT += 1, oiPNT += 1) {
        *oiPNT = (uint8_t)*iPNT;
    }
    free(inImg->image);
    return(1);
}