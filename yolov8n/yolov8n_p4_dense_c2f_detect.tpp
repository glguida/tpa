inst 880 yolov8n_p4_dense_c2f_input_source_pdef
inst 881 yolov8n_p4_dense_c2f_pdef
inst 882 yolov8n_p4_dense_c2f_detect_pdef
inst 883 yolov8n_p4_dense_c2f_detect_checker_pdef

conn 880 0 881 0 307200
conn 881 1 882 0 204800
conn 881 2 883 0 160
conn 882 1 883 1 192
