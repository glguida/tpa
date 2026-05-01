inst 500 yolov5n_src_input_pdef
inst 501 yolov5n_p0_pdef
inst 502 yolov5n_p1_pdef
inst 503 yolov5n_fork2_pdef
inst 504 yolov5n_p2_pdef
inst 505 yolov5n_fork2_pdef
inst 506 yolov5n_p3_pdef
inst 507 yolov5n_fork2_pdef
inst 508 yolov5n_p4_pdef
inst 509 yolov5n_fork2_pdef
inst 510 yolov5n_p5_pdef
inst 511 yolov5n_fork2_pdef
inst 512 yolov5n_p6_pdef
inst 513 yolov5n_p7_pdef
inst 514 yolov5n_fork2_pdef
inst 515 yolov5n_p8_pdef
inst 516 yolov5n_p9_pdef
inst 517 yolov5n_p10_pdef
inst 518 yolov5n_demo_sink_pdef

conn 500 0 501 0 1228800
conn 501 1 502 0 819200
conn 502 1 503 0 409600
conn 503 1 504 0 409600
conn 503 2 510 1 409600
conn 504 1 505 0 204800
conn 505 1 506 0 204800
conn 505 2 508 1 204800
conn 506 1 507 0 51200
conn 507 1 508 0 51200
conn 507 2 516 1 51200
conn 508 2 509 0 102400
conn 509 1 510 0 102400
conn 509 2 513 1 102400
conn 510 2 511 0 409600
conn 511 1 512 0 409600
conn 511 2 513 0 409600
conn 513 2 514 0 204800
conn 514 1 515 0 204800
conn 514 2 516 0 204800
conn 516 2 517 0 102400

conn 512 1 518 0 409600
conn 512 2 518 1 512000
conn 515 1 518 2 102400
conn 515 2 518 3 128000
conn 517 1 518 4 25600
conn 517 2 518 5 32000
