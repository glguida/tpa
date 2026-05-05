inst 880 yolov8n_p4_dense_c2f_input_source_pdef
inst 881 yolov8n_p4_dense_c2f_pdef
inst 910 yolov8n_p4_p5_model19_pdef
inst 911 yolov8n_p4_p5_model9_source_pdef
inst 912 yolov8n_p4_p5_concat_pdef
inst 913 yolov8n_p4_p5_checker_pdef

conn 880 0 881 0 307200
conn 881 1 910 0 204800
conn 881 2 913 0 160
conn 910 1 912 0 51200
conn 910 2 913 1 160
conn 911 0 912 1 102400
conn 912 2 913 2 153600
conn 912 3 913 3 160
