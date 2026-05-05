inst 890 yolov8n_p3_dense_c2f_input_source_pdef
inst 891 yolov8n_p3_dense_c2f_pdef
inst 892 yolov8n_p3_dense_c2f_detect_pdef
inst 880 yolov8n_p4_dense_c2f_input_source_pdef
inst 881 yolov8n_p4_dense_c2f_pdef
inst 882 yolov8n_p4_dense_c2f_detect_pdef
inst 870 yolov8n_p5_dense_c2f_input_source_pdef
inst 871 yolov8n_p5_dense_c2f_pdef
inst 872 yolov8n_p5_dense_c2f_detect_pdef
inst 900 yolov8n_dense_c2f_detect_downstream_checker_pdef
conn 890 0 891 0 1228800
conn 891 1 892 0 409600
conn 891 2 900 0 160
conn 892 1 900 1 192
conn 880 0 881 0 307200
conn 881 1 882 0 204800
conn 881 2 900 2 160
conn 882 1 900 3 192
conn 870 0 871 0 153600
conn 871 1 872 0 102400
conn 871 2 900 4 160
conn 872 1 900 5 192
