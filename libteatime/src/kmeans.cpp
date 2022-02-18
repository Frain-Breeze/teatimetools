#include "kmeans.hpp"
#include <time.h>
#include <stdlib.h>
#include <cmath>
#include <cstring>

#include "logging.hpp"

inline float sqrt1(float n)
{
	int ni;
	memcpy(&ni, &n, sizeof(n));
	int ui = 0x2035AD0C + (ni >> 1);
	float u;
	memcpy(&u, &ui, sizeof(u));
	return n / u + u * 0.25f;
}

inline float euclid_distance(KCOLF& c1, KCOLF& c2) {
	
	float tosqrt = (c1.a - c2.a) * (c1.a - c2.a)
		+ (c1.r - c2.r) * (c1.r - c2.r)
		+ (c1.g - c2.g) * (c1.g - c2.g)
		+ (c1.b - c2.b) * (c1.b - c2.b);
		
	return sqrt1(tosqrt);
}

/*float euclid_distance(KCOLF c1, KCOLF c2) {
 return std::sqrt(std::pow(c1.a - c2.a, 2)
 + std::pow(c1.r - c2.r, 2)
 + std::pow(c1.g - c2.g, 2)
 + std::pow(c1.b - c2.b, 2));
 }*/

void assign_cluster_centres(std::vector<KCOLF>& in_image, int width, int height, std::vector<int>& out_index, std::vector<KCOLF>& points) {
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			KCOLF& cf = in_image[(y * width) + x];
			
			float min_dist = euclid_distance(points[0], cf);
			for(int k = 1; k < points.size(); k++) {
				float distance = euclid_distance(points[k], cf);
				if(distance < min_dist) {
					min_dist = distance;
					out_index[(y * width) + x] = k;
				}
			}
		}
	}
}


//returns amount of points that have a group attached
size_t compute_points(std::vector<KCOLF>& in_image, std::vector<int>& index, std::vector<KCOLF>& points) {
	
	int k_matched = 0;
	
	for(int k = 0; k < points.size(); k++) {
		float mean_a = 0, mean_r = 0, mean_g = 0, mean_b = 0;
		int matching = 0;
		for(int i = 0; i < in_image.size(); i++) {
			if(index[i] == k) {
				KCOLF& cf = in_image[i];
				mean_a += cf.a;
				mean_r += cf.r;
				mean_g += cf.g;
				mean_b += cf.b;
				matching++;
			}
		}
		if(matching) {
			k_matched++;
			
			mean_a /= (float)matching;
			mean_r /= (float)matching;
			mean_g /= (float)matching;
			mean_b /= (float)matching;
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
	
	std::vector<KCOLF> in_normalized(in_image.size());
	for(int i = 0; i < in_image.size(); i++) {
		in_normalized[i].a = ((float)in_image[i].a) / 255.0;
		in_normalized[i].r = ((float)in_image[i].r) / 255.0;
		in_normalized[i].g = ((float)in_image[i].g) / 255.0;
		in_normalized[i].b = ((float)in_image[i].b) / 255.0;
	}
	
	out_index.resize(in_image.size());
	
	std::vector<KCOLF> points;
	points.resize(k_count);
	
	srand(time(NULL));
	//set starting points to be random colors
	//TODO: actually properly divide them systematically, so that images are reproducible
	for(int i = 0; i < k_count; i++) {
		points[i].r = rand() / (float)RAND_MAX;
		points[i].g = rand() / (float)RAND_MAX;
		points[i].b = rand() / (float)RAND_MAX;
		points[i].a = rand() / (float)RAND_MAX;
	}
	
	assign_cluster_centres(in_normalized, width, height, out_index, points);
	LOGVER("done assigning initial clusters");
	
	if(training_rounds == 0){
		for(int i = 0;; i++) {
			size_t k_matched = compute_points(in_normalized, out_index, points);
			eliminate_duplicates(points);
			assign_cluster_centres(in_normalized, width, height, out_index, points);
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
			compute_points(in_normalized, out_index, points);
			eliminate_duplicates(points);
			LOGVER("done computing points for round %d", i + 1);
			assign_cluster_centres(in_normalized, width, height, out_index, points);
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

