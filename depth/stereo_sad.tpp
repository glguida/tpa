inst 14010 depth_stereo_sad_source_pdef
inst 14020 depth_stereo_sad_worker_pdef
inst 14021 depth_stereo_sad_worker_pdef
inst 14022 depth_stereo_sad_worker_pdef
inst 14023 depth_stereo_sad_worker_pdef
inst 14030 depth_stereo_sad_checker_pdef

conn 14010 0 14020 0 3904
conn 14010 1 14021 0 3904
conn 14010 2 14022 0 3904
conn 14010 3 14023 0 3904
conn 14020 1 14030 0 1600
conn 14021 1 14030 1 1600
conn 14022 1 14030 2 1600
conn 14023 1 14030 3 1600
