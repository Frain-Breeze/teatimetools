#include "kmeans.hpp"
#include <time.h>
#include <stdlib.h>
#include <cmath>

#include "logging.hpp"

double euclid_distance(KCOLF c1, KCOLF c2) {
    return std::sqrt(std::pow(c1.a - c2.a, 2)
        + std::pow(c1.r - c2.r, 2)
        + std::pow(c1.g - c2.g, 2)
        + std::pow(c1.b - c2.b, 2));
}

void assign_cluster_centres(std::vector<KCOL>& in_image, int width, int height, std::vector<int>& out_index, std::vector<KCOLF>& points) {
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            KCOL c = in_image[(y * width) + x];
            KCOLF cf;
            cf.a = ((float)c.a) / 255.0;
            cf.r = ((float)c.r) / 255.0;
            cf.g = ((float)c.g) / 255.0;
            cf.b = ((float)c.b) / 255.0;
            double min_dist = euclid_distance(points[0], cf);
            for(int k = 1; k < points.size(); k++) {
                double distance = euclid_distance(points[k], cf);
                if(distance < min_dist) {
                    min_dist = distance;
                    out_index[(y * width) + x] = k;
                }
            }
        }
    }
}


//returns amount of points that have a group attached
size_t compute_points(std::vector<KCOL>& in_image, std::vector<int>& index, std::vector<KCOLF>& points) {

    int k_matched = 0;

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
        if(matching) {
            k_matched++;

            mean_a /= ((double)matching * 255.0);
            mean_r /= ((double)matching * 255.0);
            mean_g /= ((double)matching * 255.0);
            mean_b /= ((double)matching * 255.0);
            points[k].a = mean_a;
            points[k].r = mean_r;
            points[k].g = mean_g;
            points[k].b = mean_b;
        }
        else {
            points[k].r = rand() / (float)RAND_MAX;
            points[k].g = rand() / (float)RAND_MAX;
            points[k].b = rand() / (float)RAND_MAX;
            points[k].a = rand() / (float)RAND_MAX;
        }
    }

    LOGVER("%d/%d points had pixels in their group.", k_matched, points.size());

    return k_matched;
}

void eliminate_duplicates(std::vector<KCOLF>& points) {
    const float duplicate_range = 1.5 / points.size();
    for(int i = 0; i < points.size(); i++) {
        for(int a = 0; a < points.size(); a++) {
            if(i == a) { continue; }
            if(points[i].a - points[a].a < duplicate_range
                && points[i].a - points[a].a > -duplicate_range
                && points[i].r - points[a].r < duplicate_range
                && points[i].r - points[a].r > -duplicate_range
                && points[i].g - points[a].g < duplicate_range
                && points[i].g - points[a].g > -duplicate_range
                && points[i].b - points[a].b < duplicate_range
                && points[i].b - points[a].b > -duplicate_range) {

                points[a].r = rand() / (float)RAND_MAX;
                points[a].g = rand() / (float)RAND_MAX;
                points[a].b = rand() / (float)RAND_MAX;
                points[a].a = rand() / (float)RAND_MAX;
                LOGVER("eliminated duplicate");
            }
        }
    }
}

float kmeans(std::vector<KCOL>& in_image, int width, int height, std::vector<int>& out_index, std::vector<KCOL>& out_palette, int k_count, int training_rounds, int max_rounds) {
    LOGBLK

    out_index.resize(in_image.size());

    std::vector<KCOLF> points;
    points.resize(k_count);

    srand(time(NULL));
    //set starting points to be random colors
    for(int i = 0; i < k_count; i++) {
        points[i].r = rand() / (float)RAND_MAX;
        points[i].g = rand() / (float)RAND_MAX;
        points[i].b = rand() / (float)RAND_MAX;
        points[i].a = rand() / (float)RAND_MAX;
    }

    assign_cluster_centres(in_image, width, height, out_index, points);
    LOGVER("done assigning initial clusters");

    if(training_rounds == 0){
        for(int i = 0;; i++) {
            size_t k_matched = compute_points(in_image, out_index, points);
            eliminate_duplicates(points);
            assign_cluster_centres(in_image, width, height, out_index, points);
            if(k_matched == points.size()) {
                break;
            }
            if((max_rounds) && i > max_rounds) {
                break;
            }
        }

    }
    else {
        for(int i = 0; i < training_rounds; i++) {
            compute_points(in_image, out_index, points);
            eliminate_duplicates(points);
            LOGVER("done computing points for round %d", i + 1);
            assign_cluster_centres(in_image, width, height, out_index, points);
            LOGVER("done assigning clusters for round  %d", i + 1);
        }
    }


    //convert colors
    out_palette.resize(k_count);
    for(int i = 0; i < k_count; i++) {
        out_palette[i].a = points[i].a * 255.0;
        out_palette[i].r = points[i].r * 255.0;
        out_palette[i].g = points[i].g * 255.0;
        out_palette[i].b = points[i].b * 255.0;
    }


    //TODO: calculate the quality
    return 1;
}

