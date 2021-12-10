#include <cstdio>
#include <sys/resource.h>
#include <cstring>
#include <sys/time.h>
#include <ffmpeg_utils.h>
#include <ftw.h>
#include <fnmatch.h>
#include "vision_utils.h"
#include <iostream>
#include <cxxopts.hpp>
extern "C" {
    #include "img_utils.h"
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}
#include "tbb/tbb.h"
using namespace tbb;
#define ntoken 6
#define KERN_X  7   // @brief Contact Kernel x len
#define KERN_Y  7   // @brief Contact Kernel y len
//bool CONTACT_KERNEL[9] = { false, true, false, true, true, true, false, true, false};   // @brief Contact Kernel for Grass Fire algorithm
bool CONTACT_KERNEL[KERN_X*KERN_Y] = {false, false, false, true, false, false, false,
                                      false, false, true, true, true, false, false,
                                      false, true, true, true, true, true, false,
                                      true, true, true, true, true, true, true,
                                      false, true, true, true, true, true, false,
                                      false, false, true, true, true, false, false,
                                      false, false, false, true, false, false, false };
size_t batch_count = 0; //@brief Number of images loaded under batch op.
size_t list_count = 0;  //@brief Number of images loaded under batch op.
char **batch_list;

/* @brief Counts number of images in a directory.
 * @param   *fpath      Path to file
 * @param   *sb
 * @return  typeFlag    Flag for file*/
static int count_explore( const char *fpath, const struct stat *sb, int typeflag ) {
    if (typeflag == FTW_F) {
        if (fnmatch("*.jpg", fpath, FNM_CASEFOLD) == 0) {
            batch_count++;
        }
    }
    return 0;
}

static int list_explore( const char *fpath, const struct stat *sb, int typeflag ) {
    if (typeflag == FTW_F) {
        if (fnmatch("*.jpg", fpath, FNM_CASEFOLD) == 0) {
            int len = (int)strlen(fpath);
            batch_list[list_count] = (char*)malloc(len*sizeof(char)+1); // Add 1 for \0
            strcpy(batch_list[list_count],fpath);
            list_count++;
        }
    }
    return 0;
}

/* @brief Function to compare strings for sorting.
 * @param   a   String 1
 * @param   b   String 2
 * @return  */
static int myCompare(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

// Setups statics for tbb batch operation
bool *tbb_img_batch::k = CONTACT_KERNEL;
uint8_t tbb_img_batch::kX = KERN_X;
uint8_t tbb_img_batch::kY = KERN_Y;

uint8_t globalThresh = 200; // @brief Global input pixel threshold to mask to.
int     globalSave = 0;     // @brief Global save logic, 1: save *.jpg, 2: save *.bof


int main (int argc ,char ** argv) {
    struct  timeval start{},end{};  // @brief Time struct for tracking computation.
    long tUs;                       // @brief uS for time calculations
    rastImage rgbInput, grayOut;
    char fOutName[11];

    struct rlimit old_lim, lim, new_lim;
    // Get old limits
    if( getrlimit(RLIMIT_NOFILE, &old_lim) != 0)
        fprintf(stderr, "Get1 %s\n", strerror(errno));

    lim.rlim_cur = 104857600;
    lim.rlim_max = 104857600;

    // Set limits
    if(setrlimit(RLIMIT_STACK, &lim) == -1) {
        printf("ERROR expanding stack. Try running as admin.\n");
        fprintf(stderr, "\nSet: %s\n", strerror(errno));
    }
    // Get new limits
    if( getrlimit(RLIMIT_STACK, &new_lim) != 0)
        fprintf(stderr, "Get2 %s\n", strerror(errno));

    try {
        // Setting Up application Options
        cxxopts::Options options(argv[0], "Finds heat sources in thermal images and returns centroids and average values of heat BLOBS.\n");
        options
                .positional_help("-i SOURCE [<args>]")
                .show_positional_help()
                .set_width(70)
                .set_tab_expansion()
                .allow_unrecognised_options()
                .add_options("Required")
                        ("i", "File [*.jpg/*.bin] or folder input", cxxopts::value<std::vector<std::string>>(), "SOURCE")
                        ("t", "Pixel threshold saturation value", cxxopts::value<uint8_t>(globalThresh)->default_value("200"));
        options
                .add_options("Optional")
                        ("b,benchmark", "Tests performance of arbitrary 25x25 to 4K images.")
                        ("c", "Compare TBB parallel and sequential")
                        ("h,help", "Print help")
                        ("o,output", "Output image map of BLOB centroids\n\t1: Output image file [*.jpg]\n\t2: Output binary output file [*.bof]", cxxopts::value<int>(globalSave))
                        ("s", "Run Sequential");
        options.add_options("*.BIF Input required")
                ("x", "Image X size", cxxopts::value<size_t>())
                ("y", "Image Y size", cxxopts::value<size_t>());
        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help({"Required","Optional","*.BIF Input required"}) << std::endl;
            exit(0);
        }
        if (argc == 1){
            std::cout << options.help({"Required","Optional","*.BIF Input required"}) << std::endl;
            exit(0);
        }
        if (result.count("b")) {
// Run Benchmark
            //8k: 7680x4320
            rastImage       benchImage, benchMap;
            rastImageBin    benchMask;
            printf("Testing 100 levels from 76x43 to 7680x4320. All Units [us]\n");
            printf("---------------------------------------------------------------------\n");
            printf("Res\t");
            //printf("%5.5s","     ");
            printf("\tS.Mask\tP.Mask\tP.RGF\tS.RGF\tS.BAVG\tP.BAVG\tP.BAVG2\tS.BCNT\tP.BCNT\tP.BCNT2\n");
            printf("---------------------------------------------------------------------\n");
            for (int x = 77, y = 43; x <= 7680; x+=77, y+=43) {
                // Setup Data
                benchImage.width = x; benchImage.height = y;
                benchImage.size = x*y; benchImage.ch = 1;
                benchImage.image = (uint8_t*)malloc(benchImage.size*sizeof(uint8_t));
                if (benchImage.image == nullptr) {printf("Unable to allocate mem (benchImage)\n");  fflush(stdout); return-1;}
                // Set all pixels to 255 (White)
                for (size_t i = 0; i < benchImage.size; i++) {
                    benchImage.image[i] = 255;
                }
                // Copy grey metadata to mask
                benchMask.width = benchImage.width; benchMask.height = benchImage.height; benchMask.ch = 1; benchMask.size = benchMask.width * benchMask.height;
                benchMask.image = (bool *)malloc((benchMask.size)*sizeof(bool));
                if (benchMask.image == nullptr) {printf("Unable to allocate mem (benchMask)\n"); fflush(stdout); return-1;}

                // Copy mask metadata to map
                benchMap.width = benchMask.width; benchMap.height = benchMask.height; benchMap.ch = 1; benchMap.size = benchMap.width * benchMap.height;
                benchMap.image = (uint8_t*)calloc((benchMap.size),sizeof(uint8_t));
                if (benchMap.image == nullptr) {printf("Unable to allocate mem (benchMap)\n");  fflush(stdout); return-1;}

                size_t  blob_cnt = 0;
                // Print Resolution
                printf("%dx%d,\t",x,y);
                if(x == 77 || x == 154) printf("\t");
                // S.Mask
                gettimeofday(&start, nullptr);
                mask_blobs(&benchImage, &benchMask, globalThresh);
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);
                //if(x == 77 || x == 154 || x == 231) printf("\t");

                // P.Mask
                gettimeofday(&start, nullptr);
                tbb_mask_blobs(&benchImage, &benchMask, globalThresh);
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);


                // P.RGF
                gettimeofday(&start, nullptr);
                blob_cnt = tbb_recursive_grass_fire(&benchMask, &benchMap, CONTACT_KERNEL, KERN_X, KERN_Y);
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);

                // Reallocate to do RGF again...
                    free(benchMask.image);
                    free(benchMap.image);

                    // Copy grey metadata to mask
                    benchMask.width = benchImage.width; benchMask.height = benchImage.height; benchMask.ch = 1; benchMask.size = benchMask.width * benchMask.height;
                    benchMask.image = (bool *)malloc((benchMask.size)*sizeof(bool));
                    if (benchMask.image == nullptr) {printf("Unable to allocate mem (benchMask)\n"); return-1;}
                    mask_blobs(&benchImage, &benchMask, globalThresh);
                    // Copy mask metadata to map
                    benchMap.width = benchMask.width; benchMap.height = benchMask.height; benchMap.ch = 1; benchMap.size = benchMap.width * benchMap.height;
                    benchMap.image = (uint8_t*)calloc((benchMap.size),sizeof(uint8_t));
                    if (benchMap.image == nullptr) {printf("Unable to allocate mem (benchMap)\n"); return-1;}


                // S.RGF
                gettimeofday(&start, nullptr);
                blob_cnt = recursive_grass_fire(&benchMask, &benchMap, CONTACT_KERNEL, KERN_X, KERN_Y);
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);

                // S.BAVG
                BLOB *blob_meta = (BLOB*)malloc(sizeof(BLOB)*blob_cnt);
                gettimeofday(&start, nullptr);
                for (size_t i = 1; i < blob_cnt; i++) {
                    blob_meta[i-1].b_avg = seq_BLOB_avg_value(&benchMap,&benchImage,i);
                }
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);

                // P.AVG
                gettimeofday(&start, nullptr);
                tbb_BLOB_avg_value(&benchMap, blob_meta, &benchImage, blob_cnt);
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);

                // P.AVG2
                gettimeofday(&start, nullptr);
                tbb_BLOB_avg_value_reduce(&benchMap, blob_meta, &benchImage, blob_cnt);
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);


                // S.BCNT
                gettimeofday(&start, nullptr);
                for (size_t i = 1; i < blob_cnt; i++) {
                    seq_BLOB_center(&benchMap, &blob_meta[i-1], i);      // Find BLOB center
                }
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);

                // P.BCNT
                gettimeofday(&start, nullptr);
                tbb_BLOB_center(&benchMap, blob_meta, blob_cnt);
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);

                // P.BCNT2
                gettimeofday(&start, nullptr);
                tbb_BLOB_center_reduce(&benchMap, blob_meta, blob_cnt);
                gettimeofday(&end, nullptr);
                tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                printf("%ld,\t", tUs); fflush(stdout);

                printf("\n");
                free(blob_meta);
                free(benchMask.image);
                free(benchMap.image);
                free(benchImage.image);
            }

            return 1;
        }
        if (result.count("i")) {
            if (!result.count("t")) {
                printf("No threshold value (t) entered, defaulting to 200.\n");
            }
            auto& ff = result["i"].as<std::vector<std::string>>();
            // JPG Input
            if (ff.back().substr(ff.back().size()-4) == ".jpg") {
                printf("*.jpg detected. Importing image... ");
                // Using stb to load image directly from *.jpg file.
                if(img_load(&rgbInput, ff.back().c_str()) != 1) {
                    return(-1);
                }
                printf("Loaded image with a width of %dpx, a height of %dpx. The original image had %d channels.\n", rgbInput.width, rgbInput.height, rgbInput.ch);
                if(rgbInput.ch == 3) {
                    // RGB image loaded. Convert to grayscale.
                    if(img_c_to_g(&rgbInput, &rgbInput) != 1) {
                        printf("broke");
                        return(-1);
                    }
                }
                // Grayscale image loaded. Copy contents of rgbInput into grayOut. This just makes code consistent later.
                // Don't need to check for one channel since img_load a few lines above checks it.
                grayOut = rgbInput;
                grayOut.image = (uint8_t *)malloc(grayOut.size);        // Need to reallocate memory
                if (grayOut.image == nullptr) {
                    printf("Ope. Unable to allocate memory for comp.\n");
                    return (0);
                }
                memcpy(grayOut.image, rgbInput.image, rgbInput.size);
                img_free(&rgbInput);    // Free source file as it is no longer needed.

                long tUs_seq;
                // Sequential Batch image load.
                if (result.count("c") || result.count("s")) {
                    printf("Running sequential... ");
                    gettimeofday(&start, nullptr);
                    single_image_process(ff.back().c_str(),globalSave,CONTACT_KERNEL, KERN_X, KERN_Y, &grayOut, globalThresh, true);
                    gettimeofday(&end, nullptr);
                    tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                    tUs_seq = tUs;
                    printf("Comp. Time:\t\t%ld [us],\t\n", tUs);
                }
                // Parallel (Default)
                if(!result.count("s")) {
                    printf("Running parallel pipeline... ");
                    gettimeofday(&start, nullptr);
                    single_image_process(ff.back().c_str(),globalSave,CONTACT_KERNEL, KERN_X, KERN_Y, &grayOut, globalThresh, false);
                    gettimeofday(&end, nullptr);
                    tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                    printf("Comp. Time:\t%ld [us],\t", tUs);
                }
                printf("\n");
                if(result.count("c")) {
                    printf("-------------------------------------------\n");
                    float perc = (1 - ((float) tUs / (float) tUs_seq)) * 100;
                    printf("Total savings: %f%%\n", perc);
                }
                img_free(&grayOut);
                return 1;
            }
            // BIF Input
            if (ff.back().substr(ff.back().size()-4) == ".bif") {
                if (!result.count("x") || ! result.count("y")) {
                    printf("error, *.bif files require -x & -y sizes\n");
                    exit(0);
                }
                grayOut.width = result["x"].as<int>();
                grayOut.height = result["y"].as<int>();
                grayOut.size = grayOut.width*grayOut.height;    // Don't need to consider channels since it should be grayscale.
                grayOut.ch = 1;                                 // We'll set the channels just in case we need it later.
                printf("*.bif detected. Importing data... ");
                FILE *file_i = fopen(ff.back().c_str(),"rb");
                if (file_i == nullptr) {
                    printf("\n Error: Unable to open \"%s\".\n",ff.back().c_str());
                    return -1;
                }
                fseek(file_i, 0L, SEEK_END);	// Go to end of file
                // Check if file size matches user input
                if(grayOut.size != ftell(file_i)) {
                    printf("\n Error: file size of %ld bytes does not match user x:y size of %ld bytes. Check file and \"-x\" & \"-y\" inputs.\n",ftell(file_i),grayOut.size);
                    return -1;
                }
                rewind(file_i);			// Return to start of file before grabbing data
                printf("File is %ld bytes... ",grayOut.size);
                grayOut.image = (uint8_t *)malloc(grayOut.size);
                if(grayOut.image == nullptr) {
                    printf("Ope. Unable to allocate memory for comp.\n");
                    return(0);
                }
                if(fread(grayOut.image,grayOut.size,1,file_i) != 1) {
                    printf("Error: Unable to read entire input file.\n");
                    free(grayOut.image);
                    return -1;
                }
                fclose(file_i);
                long tUs_seq;
                // Sequential Batch image load.
                if (result.count("c") || result.count("s")) {
                    printf("Running sequential... ");
                    gettimeofday(&start, nullptr);
                    single_image_process(ff.back().c_str(),globalSave,CONTACT_KERNEL, KERN_X, KERN_Y, &grayOut, globalThresh, true);
                    gettimeofday(&end, nullptr);
                    tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                    tUs_seq = tUs;
                    printf("Comp. Time:\t\t%ld [us],\t\n", tUs);
                }
                // Parallel (Default)
                if(!result.count("s")) {
                    printf("Running parallel pipeline... ");
                    gettimeofday(&start, nullptr);
                    single_image_process(ff.back().c_str(),globalSave,CONTACT_KERNEL, KERN_X, KERN_Y, &grayOut, globalThresh, false);
                    gettimeofday(&end, nullptr);
                    tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                    printf("Comp. Time:\t%ld [us],\t", tUs);
                }
                printf("\n");
                if(result.count("c")) {
                    printf("-------------------------------------------\n");
                    float perc = (1 - ((float) tUs / (float) tUs_seq)) * 100;
                    printf("Total savings: %f%%\n", perc);
                }
                img_free(&grayOut);
                return 1;
            }
            // MOV Input
            if (ff.back().substr(ff.back().size()-4) == ".mov") {
                int frame_width, frame_height;
                unsigned char* frame_data;
                // TODO: ADD mov to frame func
                if (!load_frame(ff.back().c_str(), &frame_width, &frame_height, &frame_data)) {
                    printf("Error\n");
                    return 0;
                }
            }
            // Folder Batch input
            if((char)ff.back().back() == '/') {
                // Load and sort file names into list:
                ftw(ff.back().c_str(),count_explore, 8);                             // Get number of files
                batch_list = (char**)malloc(batch_count*sizeof(char*));
                if(batch_list == nullptr) {printf("Cannot allocate memory."); return -1;}
                ftw(ff.back().c_str(),list_explore, 8);
                qsort(batch_list, batch_count, sizeof(const char*), myCompare);         // Sort by file name
                // Grab first image data for console info
                rastImage testFirst;
                img_load(&testFirst,batch_list[0]);
                printf("Loaded %ld images with a width of %dpx, a height of %dpx. The original images had %d channels.\n", batch_count, testFirst.width, testFirst.height, testFirst.ch);
                img_free(&testFirst);
                long tUs_seq;
                // Sequential Batch image load.
                if (result.count("c") || result.count("s")) {
                    printf("Running sequential... ");
                    gettimeofday(&start, nullptr);
                    seq_img_batch(CONTACT_KERNEL, KERN_X, KERN_Y, batch_count, globalThresh, batch_list);
                    gettimeofday(&end, nullptr);
                    tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                    printf("Comp. Time:\t\t%ld [us],\t", tUs);
                    printf("AVG. Frame Time: %ld [us].\n", tUs/batch_count);
                    tUs_seq = tUs;
                }
                // Parallel (Default)
                if(!result.count("s")) {
                    printf("Running parallel pipeline... ");
                    fflush(stdout);
                    gettimeofday(&start, nullptr);
                    tbb_img_batch batch_job(batch_count, batch_list, globalThresh);
                    batch_job(ntoken);
                    gettimeofday(&end, nullptr);
                    tUs = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
                    printf("Comp. Time:\t%ld [us],\t", tUs);
                    printf("AVG. Frame Time: %ld [us].\n", tUs / batch_count);
                }
                if(result.count("c")) {
                    printf("-------------------------------------------\n");
                    float perc = (1 - ((float) tUs / (float) tUs_seq)) * 100;
                    printf("Total savings: %f%%\n", perc);
                }
                // Cleanup
                for (size_t i = 0; i < batch_count; i++) {
                    free(batch_list[i]);
                }
                free(batch_list);
            }
        } else {
            printf("No source file listed\n");
            exit(0);
        }
    }
    catch (const cxxopts::OptionException& e) {
        std::cout << "error parsing options: " << e.what() << std::endl;
        exit(1);
    }
    return 0;
}