

#include <algorithm>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <vector>
#include <set>
#include <string>
#include "Tensor.h"
#include "Model.h"


extern "C" {
    void run_model()
    {

        /*
         * Status LoadSavedModel(const SessionOptions& session_options,
                      const RunOptions& run_options, const string& export_dir,
                      const std::unordered_set<string>& tags,
                      SavedModelBundleLite* const bundle);
         */
        Model model("unet_trained/frozen_model.pb");
        //model.init();
        model.restore("unet_trained/model.ckpt");

        cv::Mat image = cv::imread("/home/axiak/Desktop/mike.png");
        if (!image.data) {
            printf("No image data\n");
            return;
        }

        const float mean_vals[3] = {0, 0, 0};
        const float norm_vals[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};

    }

    int main(int argc, char ** argv)
    {
        run_model();
        printf("hi\n");
        return 0;
    }
}
