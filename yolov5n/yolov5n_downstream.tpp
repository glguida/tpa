inst 500 yolov5n_src_skip_p5_pdef
inst 501 yolov5n_src_skip_p4_pdef
inst 502 yolov5n_src_skip_p3_pdef
inst 503 yolov5n_fork2_pdef
inst 504 yolov5n_p4_pdef
inst 505 yolov5n_fork2_pdef
inst 506 yolov5n_p5_pdef
inst 507 yolov5n_fork2_pdef
inst 508 yolov5n_p6_pdef
inst 509 yolov5n_p7_pdef
inst 510 yolov5n_fork2_pdef
inst 511 yolov5n_p8_pdef
inst 512 yolov5n_p9_pdef
inst 513 yolov5n_p10_pdef

conn 500 0 503 0 51200
conn 503 1 504 0 51200
conn 503 2 512 1 51200

conn 501 0 504 1 204800
conn 504 2 505 0 102400
conn 505 1 506 0 102400
conn 505 2 509 1 102400

conn 502 0 506 1 409600
conn 506 2 507 0 409600
conn 507 1 508 0 409600
conn 507 2 509 0 409600

conn 509 2 510 0 204800
conn 510 1 511 0 204800
conn 510 2 512 0 204800

conn 512 2 513 0 102400
