inst 880 yolov8n_p4_dense_c2f_input_source_pdef
inst 881 yolov8n_p4_dense_c2f_pdef
inst 910 yolov8n_p4_p5_model19_pdef
inst 911 yolov8n_p4_p5_model9_source_pdef
inst 912 yolov8n_p4_p5_concat_pdef
inst 871 yolov8n_p5_dense_c2f_pdef
inst 872 yolov8n_p5_dense_c2f_detect_pdef
inst 914 yolov8n_p4_p5_neck_tail_p5_detect_checker_pdef

conn 880 0 881 0 307200
conn 881 1 910 0 204800
conn 881 2 914 0 160
conn 910 1 912 0 51200
conn 910 2 914 1 160
conn 911 0 912 1 102400
conn 912 2 871 0 153600
conn 912 3 914 2 160
conn 871 1 872 0 102400
conn 871 2 914 3 160
conn 872 1 914 4 192
