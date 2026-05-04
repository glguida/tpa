inst 12010 attention_qkv_gen_pdef
inst 12100 attention_score_pdef
inst 12101 attention_score_pdef
inst 12102 attention_score_pdef
inst 12103 attention_score_pdef
inst 12200 attention_softmax_pdef
inst 12201 attention_softmax_pdef
inst 12202 attention_softmax_pdef
inst 12203 attention_softmax_pdef
inst 12300 attention_output_pdef

conn 12010 0 12100 0 3136
conn 12010 1 12101 0 3136
conn 12010 2 12102 0 3136
conn 12010 3 12103 0 3136
conn 12100 1 12200 0 2112
conn 12101 1 12201 0 2112
conn 12102 1 12202 0 2112
conn 12103 1 12203 0 2112
conn 12200 1 12300 0 2112
conn 12201 1 12300 1 2112
conn 12202 1 12300 2 2112
conn 12203 1 12300 3 2112
