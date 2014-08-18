gprof Perf/datacon_dnp2rc1 gmon.out | python gprof2dot.py -s | dot -Tsvgz -operf.svgz
