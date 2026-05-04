inst 820 yolov8n_p3_source_pdef
inst 821 yolov8n_p4_source_pdef
inst 822 yolov8n_detect_p5_source_pdef
inst 823 yolov8n_p3_detect_pdef
inst 824 yolov8n_p4_detect_pdef
inst 825 yolov8n_detect_p5_pdef
inst 826 yolov8n_detect_checker_pdef

conn 820 0 823 0 409600
conn 821 0 824 0 204800
conn 822 0 825 0 102400
conn 823 1 826 0 192
conn 824 1 826 1 192
conn 825 1 826 2 192
