#include "vision_utils.h"
extern "C" {
#include "img_utils.h"
}
#include "tbb/tbb.h"
using namespace tbb;

void copy_mask(rastImage *inputImage, rastImageBin *outBool) {
    inputImage->width = outBool->width;
    inputImage->height = outBool->height;
    outBool->ch = 1;
    outBool->size = outBool->width * outBool->height;
    outBool->image = (bool *)malloc(outBool->size);
}

void mask_blobs(rastImage *inputImage, rastImageBin *outImage, uint8_t thresh) {
    uint8_t *img = inputImage->image;
    bool *imgOut = outImage->image;
    for (size_t i = 0; i < outImage->size; i++, img++, imgOut++) {
        if (*img < thresh) *imgOut = false; else *imgOut = true;
    }
}

void recursive_burn(uint8_t burnID, rastImageBin *boolImg, rastImage *mapStruct, uint8_t *map, const bool *kernel, const uint8_t kX, const uint8_t kY) {
    //int kCX = kX / 2; int kCY = kY / 2;         // Grab Center of kernel
    //uint8_t *mapPoint = map;                    // Copy pointers for recursion
    //uint8_t *KmapPoint;                         // @brief Kernel pixel location on output
    //bool    *KboolPoint;                         // @brief Kernel pixel location on Boolean source image
    //bool    *kerPoint = kernel;
    /* Center place center of kernel (X & Y) directly over current *map pixel
     *  E.G. kernel offset:
     * | 0 1 2 |    | -4 -3 -2 |
     * | 3 4 5 | -> | -1  0  1 |
     * | 6 7 8 |    |  2  3  4 |
     */
    for (int8_t i = ((kX / 2)-(kX-1)); i <= (kX / 2); ++i) {      // Kernel: rows
        for (int8_t j = ((kY / 2)-(kY-1)); j <= (kY / 2); ++j) {  // Kernel: columns
            //KmapPoint   = mapPoint + i*mapStruct->width + j;
            //KboolPoint  =  boolImg->image + (KmapPoint-mapStruct->image);
            // Make sure mapped kernel pixel is within the mapping image frame
            if((uintptr_t)(map + i*mapStruct->width + j) >= (uintptr_t)mapStruct->image && (uintptr_t)(map + i*mapStruct->width + j) < (uintptr_t)mapStruct->image + (uintptr_t)mapStruct->size) {
                /* Check that:  1. Mapping pixel isn't already set
    *              2. Pixel in boolImg isn't burnt (set to 1)
    *              3. Pixel on contact kernel is on*/
                //int tKoff = i + (kX/2) + j + (kY/2);    // Set kernel offset back to top left.
                // Running this check before "edge pixel check" reduces process time up to 20%
                if(*(map + i*mapStruct->width + j) == 0 && *(boolImg->image + ((map + i*mapStruct->width + j)-mapStruct->image)) == 1 && *(kernel+(i + (kX/2) + j + (kY/2)))) {
                    // Map current kernel row at center line (K[0,i]) and find which row it maps to on the output
                    //size_t CCrow = ((map + i*mapStruct->width   ) - mapStruct->image) / mapStruct->width;
                    // Check if current operating kernel pixel (K[j,i]) is on the same row. If not, then skip.
                    //size_t Crow = ((map + i*mapStruct->width + j) - mapStruct->image) / mapStruct->width;
                    if ((((map + i*mapStruct->width   ) - mapStruct->image) / mapStruct->width) == (((map + i*mapStruct->width + j) - mapStruct->image) / mapStruct->width)) {    // This step prevents from the kernel from wrapping around the edges.
                        *(map + i*mapStruct->width + j)  = burnID;   // Set BLOB id to mapped output
                        *(boolImg->image + ((map + i*mapStruct->width + j)-mapStruct->image)) = false;    // Burn input pixel
                        // Recursively iterate again and center kernel on current pixel
                        // Don't iterate when k[0,0]
                        if (i != 0 || j != 0) recursive_burn(burnID, boolImg, mapStruct, (map + i*mapStruct->width + j), kernel, kX, kY);
                    }
                }
            }
        }
    }
}

int recursive_grass_fire(rastImageBin *inputImage, rastImage *mapImage, const bool *kernel, const uint8_t kX, const uint8_t kY) {
    int sX, sY;                                         // @brief Input image size
    bool *img = inputImage->image;                      // @brief Input image pointer
    size_t imgSize = inputImage->size;                  // @brief Input image size
    sX = inputImage->width; sY = inputImage->height;
    if (!img || !kernel) return 0;       // Check for empty data
    if (sX <= 0 || kX <= 0) return 0;   // Check img and kernel size
    uint8_t *mapPtr = mapImage->image;
    // Scan through image and find first pixel that is 'on'.
    for ( ; img < (inputImage->image + imgSize); img++, mapPtr++) {
        if (*img) {
            //*mapPtr = 1; // Don't Need to do this?
            break;
        }
    }
    // Start at first 'on' pixels. Iterate until all pixels are burnt.
    uint8_t burnID = BLOB_START_ID;
    for ( ; img < (inputImage->image + imgSize); img++, mapPtr++) {
        if (*img) {
            burnID++;
            recursive_burn(burnID, inputImage, mapImage, mapPtr, kernel, kX, kY);
        }
    }
    return burnID;
}

int save_blobs_to_img(const char *fName, bool imageFormat, rastImage *inMap, BLOB *inBLOBS, size_t blobCNT) {
    rastImage output;
    output.width = inMap->width; output.height = inMap->height; output.size = inMap->size; output.ch = inMap->ch;
    output.image = (uint8_t*)calloc(output.size,sizeof(uint8_t));
    if(output.image == nullptr) {
        printf("Unable to allocate mem (save_blobs_to_img)"); fflush(stdout);
        return -1;
    }
    char newName[] = "o_";/*
    strcat(newName,fName);
    int len = strlen(newName);
    char* substr = (char*)malloc(len);
    memcpy(substr, newName, len-4 );*/
    for (size_t i = 0; i <= blobCNT; i++) {
        size_t dimension = inMap->width*inBLOBS->BY + inBLOBS->BX;
        if (dimension > output.size) {
            printf("Error saving file. BLOB centroid calculated outside image region.");
            return -1;
        }
        output.image[dimension] = inBLOBS->b_avg;
    }
    if (imageFormat) {
        //strcat(substr,".jpg");
        img_save("OUT.jpg",&output);
    } else {
        //strcat(substr,".bof");
        FILE *file_o;
        file_o = fopen("OUT.bof","w+b");
        fwrite(output.image,sizeof(uint8_t),output.size,file_o);
        if(ferror(file_o)){
            printf("Error: Unable to write output file.\n");
            return -1;
        }
        fclose(file_o);
    };
    free(output.image);
    return 1;
}

uint8_t seq_BLOB_avg_value(rastImage *map, rastImage *greyInput, uint8_t blobID) {
    float tempSum = 0;
    size_t cnt = 0;
    for (size_t i = 0; i < map->size; i++) {
        if(map->image[i] == blobID) {
            tempSum += (float)(greyInput->image[i]);
            cnt++;
        }
    }
    tempSum /= (float)cnt;
    return (uint8_t)tempSum;
}

int single_image_process(const char *fName, int imageFormat, bool *k, uint8_t kX, uint8_t kY, rastImage *inGrey, uint8_t inThresh, bool sequential) {
    class vision_bunch {
    public:
        rastImageBin    *mask;
        rastImage       *grey;
        rastImage       *map;
        size_t          blob_cnt;
        uint8_t         thresh;
    };
    vision_bunch t{};
    t.grey = inGrey; t.thresh = inThresh;
    t.mask = (rastImageBin*)malloc(sizeof(rastImageBin));
    t.map = (rastImage*)malloc(sizeof(rastImage));
    if (t.grey == nullptr || t.mask == nullptr || t.map == nullptr) {printf("Unable to allocate mem\n"); return -1;}
    // Copy grey metadata to mask
    t.mask->width = t.grey->width; t.mask->height = t.grey->height; t.mask->ch = 1; t.mask->size = t.mask->width * t.mask->height;
    t.mask->image = (bool *)malloc((t.mask->size)*sizeof(bool));
    // Copy mask metadata to map
    t.map->width = t.mask->width; t.map->height = t.mask->height; t.map->ch = 1; t.map->size = t.map->width * t.map->height;
    t.map->image = (uint8_t*)calloc((t.map->size),sizeof(uint8_t));
    if(t.map->image == nullptr) {
        printf("Unable to allocate mem (t.map->image)"); fflush(stdout);
        return -1;
    }
    if(t.mask->image == nullptr) {
        printf("Unable to allocate mem (t.mask->image)"); fflush(stdout);
        return -1;
    }
    BLOB *blob_meta;
    if (sequential) {
        mask_blobs(t.grey, t.mask, t.thresh);
        t.blob_cnt = recursive_grass_fire(t.mask, t.map, k, kX, kY);
        img_save("test1.jpg",t.grey);
        img_save("test2.jpg",t.map);
        blob_meta = (BLOB*)malloc(sizeof(BLOB)*t.blob_cnt);
        if(blob_meta == nullptr) {
            printf("Unable to allocate mem (blob_meta"); fflush(stdout);
            return(-1);
        }
        for (size_t i = 1; i <= t.blob_cnt; i++) {
            blob_meta[i-1].b_avg = seq_BLOB_avg_value(t.map,t.grey,i);
            seq_BLOB_center(t.map, &blob_meta[i-1], i);
        }
    } else {
        tbb_mask_blobs(t.grey, t.mask, t.thresh);
        t.blob_cnt = recursive_grass_fire(t.mask, t.map, k, kX, kY);
        blob_meta = (BLOB*)malloc(sizeof(BLOB)*t.blob_cnt);
        if(blob_meta == nullptr) {
            printf("Unable to allocate mem (blob_meta"); fflush(stdout);
            return(-1);
        }
        tbb_BLOB_avg_value(t.map, blob_meta, t.grey, t.blob_cnt);
        tbb_BLOB_center(t.map, blob_meta, t.blob_cnt);
    }
    if (imageFormat == 1) {
        save_blobs_to_img(fName,true,t.map,blob_meta,t.blob_cnt);
    } else if (imageFormat == 2) {
        save_blobs_to_img(fName,false,t.map,blob_meta,t.blob_cnt);
    }
    free(blob_meta);
    free(t.mask->image);
    free(t.map->image);
    free(t.mask);
    free(t.map);
    return 1;
}

void seq_BLOB_center(rastImage *map, BLOB *inBLOB, uint8_t blobID) {
    size_t cnt = 0, tX = 0, tY = 0;
    for (size_t i = 0; i < map->size; i++) {
        if (map->image[i] == blobID) {
            tY += i/map->width;
            tX += i%map->width;
            cnt++;
        }
    }
    if(cnt == 0) {
        inBLOB->BX = 0;
        inBLOB->BY = 0;
    } else {
        inBLOB->BX = tX / cnt;
        inBLOB->BY = tY / cnt;
    }
}

int seq_img_batch(bool *k, uint8_t kX, uint8_t kY, size_t batch_cnt, uint8_t inThresh, char **batch_list) {
    class vision_bunch {
    public:
        rastImageBin    *mask;
        rastImage       *map;
        size_t          blob_cnt;
        uint8_t         thresh;
    };
    vision_bunch t{};
    auto *grey = (rastImage*)malloc(sizeof(rastImage));
    t.mask = (rastImageBin*)malloc(sizeof(rastImageBin));
    t.map = (rastImage*)malloc(sizeof(rastImage));
    if (grey == nullptr || t.mask == nullptr || t.map == nullptr) {printf("Unable to allocate mem\n"); return-1;}
    for (size_t c = 0; c < batch_cnt; c++) {
        if(img_load(grey, batch_list[c]) != 1) {
            return(-1);
        }
        if(grey->ch != 1) {
            if(img_c_to_g(grey, grey) != 1) {
                printf("Failure: Converting to grayscale.\n");
                return(-1);
            }
        }
        // Copy grey metadata to mask
        t.mask->width = grey->width; t.mask->height = grey->height; t.mask->ch = 1; t.mask->size = t.mask->width * t.mask->height;
        t.mask->image = (bool *)malloc((t.mask->size)*sizeof(bool));
        if(t.mask->image == nullptr) {
            printf("Unable to allocate mem (t.mask->image: my_in: %ld).",c); fflush(stdout);
            return 0;
        }
        // Copy mask metadata to map
        t.map->width = t.mask->width; t.map->height = t.mask->height; t.map->ch = 1; t.map->size = t.map->width * t.map->height;
        t.map->image = (uint8_t*)calloc((t.map->size),sizeof(uint8_t));
        if(t.map->image == nullptr) {
            printf("Unable to allocate mem (t.map->image: my_in: %ld).",c); fflush(stdout);
            return 0;
        }
        t.blob_cnt = 0; t.thresh = inThresh;
        mask_blobs(grey, t.mask, t.thresh);                                 // Mask BLOBs
        t.blob_cnt = recursive_grass_fire(t.mask, t.map, k, kX, kY);        // Run Grass Fire
        BLOB *blob_meta = (BLOB*)malloc(sizeof(BLOB)*t.blob_cnt);
        if(blob_meta == nullptr) {
            printf("Unable to allocate mem (t.grass_fire_transf:"); fflush(stdout);
            return(-1);
        }
        for (size_t i = 1; i <= t.blob_cnt; i++) {
            blob_meta[i-1].b_avg = seq_BLOB_avg_value(t.map,grey,i);            // Calculate BLOB AVG values
            seq_BLOB_center(t.map, &blob_meta[i-1], i);      // Find BLOB center
        }

        free(blob_meta);
        img_free(grey);
        //img_save("test22.jpg",t.map);
        free(t.mask->image);
        free(t.map->image);
    }
    free(grey);
    free(t.mask);
    free(t.map);
    return 1;
}

void tbb_BLOB_avg_value(rastImage *map, BLOB *blobMeta, rastImage *greyInput, size_t blobCNT) {
    parallel_for(size_t(1), blobCNT, [&](size_t i) {
        float tempSum = 0;
        size_t cnt = 0;
        for (size_t j = 0; j < map->size; j++) {
            if(map->image[j] == i) {
                tempSum += (float)(greyInput->image[j]);
                cnt++;
            }
        }
        tempSum /= (float)cnt;
        blobMeta[i].b_avg = (int)tempSum;
    });
}

tbb_BLOB_avg_value_func::tbb_BLOB_avg_value_func(rastImage *map, rastImage *greyInput, uint8_t blobID) {
    myMap = map; MyGrey = greyInput; myBlobID = blobID; myAVG = 0;
};
tbb_BLOB_avg_value_func tbb_BLOB_avg_value_func::operator()(const blocked_range<size_t> &r) const{
    rastImage *map  = myMap;
    rastImage *grey = MyGrey;
    float AVG = myAVG;
    size_t cnt = 0;
    for (size_t i=r.begin(); i!=r.end(); ++i ) {
        if(map->image[i] == myBlobID) {
            AVG += (float)(grey->image[i]);
            cnt++;
        }
    }
};
void tbb_BLOB_avg_value_func::join(const tbb_BLOB_avg_value_func &y) {
    myAVG += y.myAVG;
    myAVG /= 2; // Need to take the average of both partial averages.
};
tbb_BLOB_avg_value_func::tbb_BLOB_avg_value_func(tbb_BLOB_avg_value_func &x, split) {
    myMap = x.myMap; MyGrey = x.MyGrey; myBlobID = x.myBlobID; myAVG = 0;
}

void tbb_BLOB_avg_value_reduce(rastImage *map, BLOB *blobMeta, rastImage *greyInput, size_t blobCNT) {
    parallel_for(size_t(1), blobCNT, [&](size_t i) {
        tbb_BLOB_avg_value_func funct(map,greyInput,i);
        parallel_reduce(blocked_range<size_t>(0,map->size), funct );
        blobMeta[i].b_avg = (int)funct.myAVG;
    });
}

void tbb_BLOB_center(rastImage *map, BLOB *blobMeta, size_t blobCNT) {
    parallel_for(size_t(1), blobCNT, [&](size_t i) {
        size_t cnt = 0, tX = 0, tY = 0;
        for (size_t j = 0; j < map->size; j++) {
            if(map->image[j] == i) {
                tY += j/map->width;
                tX += j%map->width;
                cnt++;
            }
        }
        if(cnt == 0) {
            blobMeta[i].BX = 0;
            blobMeta[i].BY = 0;
        } else {
            blobMeta[i].BX = tX / cnt;
            blobMeta[i].BY = tY / cnt;
        }
    });
}

tbb_BLOB_center_func::tbb_BLOB_center_func(rastImage *map, uint8_t blobID) {
    myMap = map; myBlobID = blobID; myX = 0; myY = 0;
};
tbb_BLOB_center_func tbb_BLOB_center_func::operator()(const blocked_range<size_t> &r) const{
    rastImage *map  = myMap;
    size_t X = myX, Y = myY;
    size_t cnt = 0;
    for (size_t i=r.begin(); i!=r.end(); ++i ) {
        if(map->image[i] == myBlobID) {
            Y += i/map->width;
            X += i%map->width;
            cnt++;
        }
    }
    if(cnt == 0) {
        X = 0;
        Y = 0;
    } else {
        X /= cnt;
        Y /= cnt;
    }
};
void tbb_BLOB_center_func::join(const tbb_BLOB_center_func &y) {
    myX += y.myX;
    myY += y.myY;
    myX /= 2; // Need to take the average of both partial averages.
    myY /= 2; // Need to take the average of both partial averages.
};
tbb_BLOB_center_func::tbb_BLOB_center_func(tbb_BLOB_center_func &x, split) {
    myMap = x.myMap; myBlobID = x.myBlobID; myX = 0; myY = 0;
}

void tbb_BLOB_center_reduce(rastImage *map, BLOB *blobMeta, size_t blobCNT) {
    parallel_for(size_t(1), blobCNT, [&](size_t i) {
        tbb_BLOB_center_func funct(map,i);
        parallel_reduce(blocked_range<size_t>(0,map->size), funct );
        blobMeta[i].BX  = (int)funct.myX;
        blobMeta[i].BY  = (int)funct.myY;
    });
}

void tbb_mask_blobs(rastImage *inputImage, rastImageBin *outImage, uint8_t thresh) {
    uint8_t *img = inputImage->image;
    bool *imgOut = outImage->image;
    parallel_for(size_t(0), inputImage->size, [&](size_t i) { // 0 <= i < n
        if (img[i] < thresh) imgOut[i] = false; else imgOut[i] = true;
    });
}

void tbb_recursive_burn(uint8_t burnID, rastImageBin *boolImg, rastImage *mapStruct, uint8_t *map, const bool *kernel, const uint8_t kX, const uint8_t kY) {
    //int kCX = kX / 2; int kCY = kY / 2;         // Grab Center of kernel
    //uint8_t *mapPoint = map;                    // Copy pointers for recursion
    //uint8_t *KmapPoint;                         // @brief Kernel pixel location on output
    //bool    *KboolPoint;                         // @brief Kernel pixel location on Boolean source image
    //bool    *kerPoint = kernel;
    /* Center place center of kernel (X & Y) directly over current *map pixel
     *  E.G. kernel offset:
     * | 0 1 2 |    | -4 -3 -2 |
     * | 3 4 5 | -> | -1  0  1 |
     * | 6 7 8 |    |  2  3  4 |
     */
    tbb::task_group g;
    for (int8_t i = ((kX / 2)-(kX-1)); i <= (kX / 2); ++i) {      // Kernel: rows
        for (int8_t j = ((kY / 2)-(kY-1)); j <= (kY / 2); ++j) {  // Kernel: columns
            //KmapPoint   = mapPoint + i*mapStruct->width + j;
            //KboolPoint  =  boolImg->image + (KmapPoint-mapStruct->image);
            // Make sure mapped kernel pixel is within the mapping image frame
            if((uintptr_t)(map + i*mapStruct->width + j) >= (uintptr_t)mapStruct->image && (uintptr_t)(map + i*mapStruct->width + j) < (uintptr_t)mapStruct->image + (uintptr_t)mapStruct->size) {
                /* Check that:  1. Mapping pixel isn't already set
 *              2. Pixel in boolImg isn't burnt (set to 1)
 *              3. Pixel on contact kernel is on*/
                //int tKoff = i + (kX/2) + j + (kY/2);    // Set kernel offset back to top left.
                // Running this check before "edge pixel check" reduces process time up to 20%
                if(*(map + i*mapStruct->width + j) == 0 && *(boolImg->image + ((map + i*mapStruct->width + j)-mapStruct->image)) == 1 && *(kernel+(i + (kX/2) + j + (kY/2)))) {
                    // Map current kernel row at center line (K[0,i]) and find which row it maps to on the output
                    //size_t CCrow = ((map + i*mapStruct->width   ) - mapStruct->image) / mapStruct->width;
                    // Check if current operating kernel pixel (K[j,i]) is on the same row. If not, then skip.
                    //size_t Crow = ((map + i*mapStruct->width + j) - mapStruct->image) / mapStruct->width;
                    if ((((map + i*mapStruct->width   ) - mapStruct->image) / mapStruct->width) == (((map + i*mapStruct->width + j) - mapStruct->image) / mapStruct->width)) {    // This step prevents from the kernel from wrapping around the edges.
                        *(map + i*mapStruct->width + j)  = burnID;   // Set BLOB id to mapped output
                        *(boolImg->image + ((map + i*mapStruct->width + j)-mapStruct->image)) = false;    // Burn input pixel
                        // Recursively iterate again and center kernel on current pixel
                        // Don't iterate when k[0,0]
                        if (i != 0 || j != 0) g.run([&]{tbb_recursive_burn(burnID, boolImg, mapStruct, (map + i*mapStruct->width + j), kernel, kX, kY);});
                    }
                }
            }
        }
    }
    g.wait();
}

int tbb_recursive_grass_fire(rastImageBin *inputImage, rastImage *mapImage, const bool *kernel, const uint8_t kX, const uint8_t kY) {
    int sX, sY;                                         // @brief Input image size
    bool *img = inputImage->image;                      // @brief Input image pointer
    size_t imgSize = inputImage->size;                  // @brief Input image size
    sX = inputImage->width; sY = inputImage->height;
    if (!img || !kernel) return 0;       // Check for empty data
    if (sX <= 0 || kX <= 0) return 0;   // Check img and kernel size
    uint8_t *mapPtr = mapImage->image;
    // Scan through image and find first pixel that is 'on'.
    for ( ; img < (inputImage->image + imgSize); img++, mapPtr++) {
        if (*img) {
            //*mapPtr = 1; // Don't Need to do this?
            break;
        }
    }
    // Start at first 'on' pixels. Iterate until all pixels are burnt.
    uint8_t burnID = BLOB_START_ID;
    for ( ; img < (inputImage->image + imgSize); img++, mapPtr++) {
        if (*img) {
            burnID++;
            tbb_recursive_burn(burnID, inputImage, mapImage, mapPtr, kernel, kX, kY);
        }
    }
    return burnID;
}

// TODO: Try moving MyPair to pointer
tbb_img_batch::tbb_img_batch(size_t batch_count, char **bListP, uint8_t inThresh) {
    batch_cnt = batch_count; batch_list = bListP; myThresh = inThresh;
}
class tbb_img_batch::my_in {
    char **batch_list;
    size_t batch_cnt;       // @brief Number of images in greyscale array
    mutable size_t c;       // @brief Current operating image
    uint8_t myThresh;       // @brief Input pixel threshold to mask to
public:
    my_in (char **list_point, size_t NVp, uint8_t inThresh): batch_list(list_point), batch_cnt(NVp), myThresh(inThresh), c(0) {}
    MyPair operator () (flow_control& fc) const {
        MyPair t{};
        if (c >= batch_cnt) {
            fc.stop();
            return t;
        } else {
            t.grey = (rastImage*)malloc(sizeof(rastImage));
            if(img_load(t.grey, batch_list[c]) != 1) {
                fc.stop();
                return t;
            }
            t.thresh = myThresh;
            c++;
            return t;
        }
    }
};
class tbb_img_batch::allocate_mem_transf {
public:
    MyPair operator() (MyPair input) const {
        // TODO: Convert to grayscale
        input.mask = (rastImageBin*)malloc(sizeof(rastImageBin));
        if(input.mask == nullptr) {
            printf("Unable to allocate mem (t.mask:"); fflush(stdout);
            tbb::task::self().cancel_group_execution();
            return input;
        }
        // Copy grey metadata to mask
        input.mask->width = input.grey->width; input.mask->height = input.grey->height; input.mask->ch = 1; input.mask->size = input.mask->width * input.mask->height;
        input.mask->image = (bool *)malloc((input.mask->size)*sizeof(bool));
        if(input.mask->image == nullptr) {
            printf("Unable to allocate mem (t.mask->image"); fflush(stdout);
            tbb::task::self().cancel_group_execution();
            return input;
        }
        input.map = (rastImage*)malloc(sizeof(rastImage));
        if(input.map == nullptr) {
            printf("Unable to allocate mem (t.map:"); fflush(stdout);
            tbb::task::self().cancel_group_execution();
            return input;
        }
        // Copy mask metadata to map
        input.map->width = input.mask->width; input.map->height = input.mask->height; input.map->ch = 1; input.map->size = input.mask->size;
        input.map->image = (uint8_t*)calloc(input.map->size,sizeof(uint8_t));
        if(input.map->image == nullptr) {
            printf("Unable to allocate mem (t.map->image:"); fflush(stdout);
            tbb::task::self().cancel_group_execution();
            return input;
        }
        input.blob_cnt = 0;
        return input;
    }
};
class tbb_img_batch::to_grayscale_transf {
public:
    MyPair operator() (MyPair input) const {
        if(input.grey->ch != 1) {
            img_c_to_g(input.grey, input.grey);
        }
        return input;
    }
};
class tbb_img_batch::mask_transf {
public:
    MyPair operator() (MyPair input) const {
        tbb_mask_blobs(input.grey, input.mask, input.thresh);
        return input;
    }
};
class tbb_img_batch::grass_fire_transf {
public:
    MyPair operator() (MyPair input) const {
        input.blob_cnt = recursive_grass_fire(input.mask, input.map, k, kX, kY);
        input.BLOBS = (BLOB*)malloc(sizeof(BLOB)*input.blob_cnt);
        if(input.BLOBS == nullptr) {
            printf("Unable to allocate mem (t.grass_fire_transf:"); fflush(stdout);
            tbb::task::self().cancel_group_execution();
        }
        return input;
    }
};
class tbb_img_batch::BLOB_avg_val {
public:
    MyPair operator() (MyPair input) const {
        tbb_BLOB_avg_value(input.map, input.BLOBS, input.grey, input.blob_cnt);
        return input;
    }
};
class tbb_img_batch::BLOB_center {
public:
    MyPair operator() (MyPair input) const {
        tbb_BLOB_center(input.map, input.BLOBS, input.blob_cnt);
        return input;
    }
};
class tbb_img_batch::p_pipe_end {
public:
    MyPair operator() (MyPair input) const {
        // Save images or something. For now free.
        //img_save("test33.jpg",input.map);
        img_free(input.grey);
        free(input.mask->image);
        free(input.map->image);
        free(input.BLOBS);
        free(input.grey);
        free(input.mask);
        free(input.map);
        return input;
    }
};
tbb_img_batch tbb_img_batch::operator()(size_t token_size) const{
    parallel_pipeline(token_size,
                      make_filter< void, MyPair>(filter::serial_in_order, my_in(batch_list,batch_cnt,myThresh))
                      & make_filter<MyPair, MyPair>(filter::parallel, allocate_mem_transf() ) // This does not appear faster not being in the first function.
                      & make_filter<MyPair, MyPair>(filter::parallel, to_grayscale_transf())
                      & make_filter<MyPair, MyPair>(filter::parallel, mask_transf())
                      & make_filter<MyPair, MyPair>(filter::parallel, grass_fire_transf())
                      & make_filter<MyPair, MyPair>(filter::parallel, BLOB_avg_val())
                      & make_filter<MyPair, MyPair>(filter::parallel, BLOB_center())
                      & make_filter<MyPair, void>(filter::serial_in_order, p_pipe_end()));
}