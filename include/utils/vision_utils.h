#ifndef ECE_4900_FINAL_M_HAIT_VISION_UTILS_H
#define ECE_4900_FINAL_M_HAIT_VISION_UTILS_H
    extern "C" {
        #include "img_utils.h"
    }
    #include "tbb/tbb.h"
    using namespace tbb;

    #define BLOB_START_ID   1   // Change to 100 when diagnosing

    // @brief Structure for image BLOB average value and centroid
    struct BLOB {
        uint8_t     BX;     // @brief BLOB centroid X location
        uint8_t     BY;     // @brief BLOB centroid Y location
        int         b_avg;  // @brief BLOB average value
    };
    class MyPair {
    public:
        rastImage       *grey;
        rastImageBin    *mask;
        rastImage       *map;
        int             blob_cnt;
        uint8_t         thresh;
        BLOB            *BLOBS;
        // TODO: Add some average value struct (x,y,data)
    };

    /** @brief Copies image meta-data to binary mask. Does not copy image.
    * @param inputImage     Image source to copy masking
    * @param outBool        Bool struct destination */
    void copy_mask(rastImage *inputImage, rastImageBin *outBool);

    /** @brief Sets any pixels in an inputImage to zero if below the threshold.
    * @param inputImage    Image to preform masking
    * @param thresh        Threshold value */
    void mask_blobs(rastImage *inputImage, rastImageBin *outImage, uint8_t thresh);

    /** @brief Recursive algorithm that sets pixel values according to BLOB ID in image map.
     *
     * All possible variables have been commented out. This prevents deep recursion from creating a stack overflow.
     * An improved design might utilise a hand-made stack stored in the heap.
     *
     * @param burnID        Value to set mapStruct image pixels when burning
     * @param boolImg       Boolean source image. Pixels are set to 0 (Burned) after setting burnID to mapping image
     * @param *mapStruct    Image structure for mapping image. (Size and image pointer locations are used)
     * @param *map          Current working mapping image pixel
     * @param *kernel       Contact kernel for burn pattern
     * @param kX            Kernel x len
     * @param kY            Kernel y len
     * */
    void recursive_burn(uint8_t burnID, rastImageBin *boolImg, rastImage *mapStruct, uint8_t *map, const bool *kernel, const uint8_t kX, const uint8_t kY);

    /** @brief Iterate grass fire algorithm over bool image
     *
     * @param   inputImage    Image to preform masking
     * @param   *mapImage    Image structure for mapping image. (Size and image pointer locations are used)
     * @param   kernel        Contact Pattern Kernel
     * @param   kX            Kernel X len
     * @param   kY            Kernel Y len
     * @return  BLOB count*/
    int recursive_grass_fire(rastImageBin *inputImage, rastImage *mapImage, const bool *kernel, const uint8_t kX, const uint8_t kY);

    /** @brief Saves BLOB metadata by creating a black image (size of inMap) and setting pixels at BLOB centroid locations to the BLOB average value.
     *
     * @param   *fName          Name of file to save
     * @param   imageFormat     True: Save to *.jpg. False: Save to *.BOF
     * @param   *inMap          Image structure for mapping image.
     * @param   *inBLOBS        Array of BLOB metadata
     * @param   blobCNT         Number of BLOBs */
    int save_blobs_to_img(const char *fName, bool imageFormat, rastImage *inMap, BLOB *inBLOBS, size_t blobCNT);

    /** @brief Averages the values of all greyInput pixels where Blob pixels equal blobID
    *
    * @param    *map            Mapping image storing masked BLOB IDs
    * @param    *greyInput      Greyscale source Image
    * @param    blobID          BLOB ID to calculate
    * @return   uint8_t Average output
    * */
    uint8_t seq_BLOB_avg_value(rastImage *map, rastImage *greyInput, uint8_t blobID);


    /** @brief Finds centroid of a single BLOB
    *
    * @param    *map            Mapping image storing masked BLOB IDs
    * @param    *BLOB           Struct to save data
    * @param    blobID          BLOB ID to calculate
    * */
    void seq_BLOB_center(rastImage *map, BLOB *inBLOB, uint8_t blobID);

    /** @brief Sequentially Preforms BLOB ID on a single image
    *
    * @param   *fName          Name of file to save
    * @param   imageFormat     True: Save to *.jpg. False: Save to *.BOF
    * @param    *k             Image Kernel
    * @param    kX             Kernel X len
    * @param    kY             Kernel Y len
    * @param    *inGrey        Input Image
    * @param    inThresh       Input pixel threshold to mask to
    * @param    sequential     Run in sequential or TBB parallel
    * @return   int Error Flags
    * */
    int single_image_process(const char *fName, int imageFormat, bool *k, uint8_t kX, uint8_t kY, rastImage *inGrey, uint8_t inThresh, bool sequential);

    /** @brief Sequentially loops through a list a files and preforms BLOB ID.
    *
    *
    * @param    *k             Image Kernel
    * @param    kX             Kernel X len
    * @param    kY             Kernel Y len
    * @param    batch_cnt      Number of images in list
    * @param    inThresh       Input pixel threshold to mask to
    * @param    **batch_list   Sorted string list of image files
    * @return   int Error Flags
    * */
    int seq_img_batch(bool *k, uint8_t kX, uint8_t kY, size_t batch_cnt, uint8_t inThresh, char **batch_list);

    /** @brief (TBB p.for) Averages the values of all greyInput pixels where Blob pixels equal blobID for all BLOBs.
    *
    * @param    *map            Mapping image storing masked BLOB IDs
    * @param    **blobMeta      Array of BLOB meta-data to save to
    * @param    *greyInput      Greyscale source Image
    * @param    blobCNT         Number of BLOBs
    * */
    void tbb_BLOB_avg_value(rastImage *map, BLOB *blobMeta, rastImage *greyInput, size_t blobCNT);

    /** @brief Class for averaging grey pixel value for a BLOB. To be used in parallel_reduce*/
    class tbb_BLOB_avg_value_func {
        rastImage   *myMap;
        rastImage   *MyGrey;
        uint8_t     myBlobID;
    public:
        float       myAVG;
        tbb_BLOB_avg_value_func (rastImage *map, rastImage *greyInput, uint8_t blobID);
        tbb_BLOB_avg_value_func operator () (const blocked_range<size_t> &r) const;
        void join (const tbb_BLOB_avg_value_func &y);
        tbb_BLOB_avg_value_func (tbb_BLOB_avg_value_func &x, split);
    };

    /** @brief (TBB p.for & reduce) Averages the values of all greyInput pixels where Blob pixels equal blobID for all BLOBs.
    *
    * @param    *map            Mapping image storing masked BLOB IDs
    * @param    *blobMeta      Array of BLOB meta-data to save to
    * @param    *greyInput      Greyscale source Image
    * @param    blobCNT         Number of BLOBs
    * */
    void tbb_BLOB_avg_value_reduce(rastImage *map, BLOB *blobMeta, rastImage *greyInput, size_t blobCNT);

    /** @brief (TBB p.for) Finds centroid of BLOBs
    *
    * @param    *map            Mapping image storing masked BLOB IDs
    * @param    *blobMeta       Array of BLOB meta-data to save to
    * @param    blobCNT         Number of BLOBs
    * */
    void tbb_BLOB_center(rastImage *map, BLOB *blobMeta, size_t blobCNT);

    /** @brief Class for Finds centroid of BLOBs. To be used in parallel_reduce*/
    class tbb_BLOB_center_func {
        rastImage   *myMap;
        uint8_t     myBlobID;
    public:
        size_t       myX, myY;
        tbb_BLOB_center_func (rastImage *map, uint8_t blobID);
        tbb_BLOB_center_func operator () (const blocked_range<size_t> &r) const;
        void join (const tbb_BLOB_center_func &y);
        tbb_BLOB_center_func (tbb_BLOB_center_func &x, split);
    };

    /** @brief (TBB p.for & Reduce) Finds centroid of BLOBs
    *
    * @param    *map            Mapping image storing masked BLOB IDs
    * @param    *blobMeta       Array of BLOB meta-data to save to
    * @param    blobCNT         Number of BLOBs
    * */
    void tbb_BLOB_center_reduce(rastImage *map, BLOB *blobMeta, size_t blobCNT);

    /** @brief (TBB) Sets any pixels in an inputImage to zero if below the threshold.
    * @param inputImage    Image to preform masking
    * @param thresh        Threshold value */
    void tbb_mask_blobs(rastImage *inputImage, rastImageBin *outImage, uint8_t thresh);

    /** @brief TBB multithreading recursive algorithm that sets pixel values according to BLOB ID in image map.
     *
     * All possible variables have been commented out. This prevents deep recursion from creating a stack overflow.
     * An improved design might utilise a hand-made stack stored in the heap.
     *
     * @param burnID        Value to set mapStruct image pixels when burning
     * @param boolImg       Boolean source image. Pixels are set to 0 (Burned) after setting burnID to mapping image
     * @param *mapStruct    Image structure for mapping image. (Size and image pointer locations are used)
     * @param *map          Current working mapping image pixel
     * @param *kernel       Contact kernel for burn pattern
     * @param kX            Kernel x len
     * @param kY            Kernel y len
     * */
    void tbb_recursive_burn(uint8_t burnID, rastImageBin *boolImg, rastImage *mapStruct, uint8_t *map, const bool *kernel, const uint8_t kX, const uint8_t kY);

    /** @brief (TBB) Iterate grass fire algorithm over bool image
    *
    * @param   inputImage    Image to preform masking
    * @param   *mapImage    Image structure for mapping image. (Size and image pointer locations are used)
    * @param   kernel        Contact Pattern Kernel
    * @param   kX            Kernel X len
    * @param   kY            Kernel Y len
    * @return  BLOB count*/
    int tbb_recursive_grass_fire(rastImageBin *inputImage, rastImage *mapImage, const bool *kernel, const uint8_t kX, const uint8_t kY);

    /** @brief TBB looping through a list a files and preforms BLOB ID.
    *
    * @param    batch_cnt   Number of images in list
    * @param    **bListP    Sorted string list of image files
    * */
    class tbb_img_batch {
        // @brief Loads images from files, converts to grayscale
        class my_in;
        // @brief Grabs heap mem for images
        class allocate_mem_transf;
        // @brief Converts image to grayscale if more than one channel
        class to_grayscale_transf;
        // @brief Sets any pixels in an inputImage to zero if below the threshold.
        class mask_transf;
        // @brief Iterate grass fire algorithm over bool image
        class grass_fire_transf;
        // @brief Averages the values of all greyInput pixels where Blob pixels equal blobID
        class BLOB_avg_val;
        // @brief Finds the centroid of each BLOB
        class BLOB_center;
        // @brief Saves Data and cleans up
        class p_pipe_end;
        public:
        static bool *k;
        static uint8_t kX; static uint8_t kY;
        size_t batch_cnt;
        char **batch_list;
        uint8_t myThresh;       // @brief Input pixel threshold to mask to
        tbb_img_batch (size_t batch_cnt, char **bListP, uint8_t inThresh);
        tbb_img_batch operator () (size_t token_size) const;
};

#endif //ECE_4900_FINAL_M_HAIT_VISION_UTILS_H
