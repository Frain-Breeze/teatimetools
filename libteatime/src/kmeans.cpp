#include "kmeans.hpp"
#include <time.h>
#include <stdlib.h>
#include <cmath>

#include "logging.hpp"

double euclid_distance(KCOL c1, KCOL c2) {
    return std::sqrt(std::pow(c1.a - c2.a, 2)
        + std::pow(c1.r - c2.r, 2)
        + std::pow(c1.g - c2.g, 2)
        + std::pow(c1.b - c2.b, 2));
}

void assign_cluster_centres(std::vector<KCOL>& in_image, int width, int height, std::vector<int>& out_index, std::vector<KCOL>& points) {
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            KCOL c = in_image[(y * width) + x];
            double min_dist = euclid_distance(points[0], c);
            for(int k = 1; k < points.size(); k++) {
                double distance = euclid_distance(points[k], c);
                if(distance < min_dist) {
                    min_dist = distance;
                    out_index[(y * width) + x] = k;
                }
            }
        }
    }
}

void compute_points(std::vector<KCOL>& in_image, std::vector<int>& index, std::vector<KCOL>& points) {
    for(int k = 0; k < points.size(); k++) {
        double mean_a = 0, mean_r = 0, mean_g = 0, mean_b = 0;
        int matching = 0;
        for(int i = 0; i < in_image.size(); i++) {
            if(index[i] == k) {
                KCOL c = in_image[i];
                mean_a += c.a;
                mean_r += c.r;
                mean_g += c.g;
                mean_b += c.b;
                matching++;
            }
        }

        mean_a /= ((double)matching);
        mean_r /= ((double)matching);
        mean_g /= ((double)matching);
        mean_b /= ((double)matching);
        points[k].a = mean_a;
        points[k].r = mean_r;
        points[k].g = mean_g;
        points[k].b = mean_b;
    }
}

float kmeans(std::vector<KCOL>& in_image, int width, int height, std::vector<int>& out_index, std::vector<KCOL>& out_palette, int k_count, int training_rounds) {
    LOGBLK

    out_palette.resize(k_count);
    out_index.resize(in_image.size());

    srand(time(NULL));
    //set starting points to be random colors
    for(int i = 0; i < k_count; i++) {
        out_palette[i].r = rand() % 255;
        out_palette[i].g = rand() % 255;
        out_palette[i].b = rand() % 255;
        out_palette[i].a = rand() % 255;
    }

    assign_cluster_centres(in_image, width, height, out_index, out_palette);
    LOGVER("done assigning initial clusters");

    for(int i = 0; i < training_rounds; i++) {
        compute_points(in_image, out_index, out_palette);
        LOGVER("done computing points for round %d", i + 1);
        assign_cluster_centres(in_image, width, height, out_index, out_palette);
        LOGVER("done assigning clusters for round  %d", i + 1);
    }


    //calculate the quality
    return 1;
}

