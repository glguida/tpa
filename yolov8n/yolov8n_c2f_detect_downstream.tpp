inst 850 yolov8n_p3_c2f_input_source_pdef
inst 851 yolov8n_p3_c2f_pdef
inst 852 yolov8n_p3_c2f_detect_pdef
inst 840 yolov8n_p4_c2f_input_source_pdef
inst 841 yolov8n_p4_c2f_pdef
inst 842 yolov8n_p4_c2f_detect_pdef
inst 830 yolov8n_p5_c2f_input_source_pdef
inst 831 yolov8n_p5_c2f_pdef
inst 832 yolov8n_p5_c2f_detect_pdef
inst 860 yolov8n_c2f_detect_downstream_checker_pdef
conn 850 0 851 0 1228800
conn 851 1 852 0 409600
conn 851 2 860 0 160
conn 852 1 860 1 192
conn 840 0 841 0 307200
conn 841 1 842 0 204800
conn 841 2 860 2 160
conn 842 1 860 3 192
conn 830 0 831 0 153600
conn 831 1 832 0 102400
conn 831 2 860 4 160
conn 832 1 860 5 192
