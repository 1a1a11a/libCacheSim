#ifndef LIBCACHESIM_ADMISSION_ADAPTSIZE_H
#define LIBCACHESIM_ADMISSION_ADAPTSIZE_H

#include "../../include/libCacheSim/request.h"
#include <unordered_map>
#include <vector>
#include <sys/types.h>
#include <cstdint>

class Adaptsize {
	public:
		Adaptsize(const uint64_t max_iteration, const uint64_t reconf_interval);
		bool admit(const request_t* req);
		void updateStats(const request_t* req, const uint64_t cache_size);
	private:
		void reconfigure();
		double modelHitRate(double log2c);

		uint64_t cache_size;
		uint64_t max_iteration;
		uint64_t reconf_interval;
		uint64_t next_reconf;
		uint64_t stat_size;
		double c_param;
		double gss_v;

		struct obj_info{
			double obj_seen_times;
			uint64_t obj_size;
		};

		std::unordered_map<obj_id_t, obj_info> interval_metadata;
		std::unordered_map<obj_id_t, obj_info> longterm_metadata;
		std::vector<double> aligned_obj_size;
		std::vector<double> aligned_obj_seen_times;
		std::vector<double> aligned_admission_probs;
};

#endif //LIBCACHESIM_ADMISSION_ADAPTSIZE_H
