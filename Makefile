debug: test example generate
release: test_release example_release generate

test:
	cc -g -o Output/itj_csv_test test.c -mavx2
example:
	cc -g -o Output/itj_csv_example_usage example_usage.c -mavx2
generate:
	cc -O2 -o Output/itj_csv_generate generate.c
test_release:
	cc -O2 -o Output/itj_csv_test test.c -mavx2
example_release:
	cc -O2 -o Output/itj_csv_example_usage example_usage.c -mavx2
