#!/bin/bash


for trc_file in *.trc; do
	number=$(basename "$trc_file" .trc | sed 's/[^0-9]*\([0-9]*\).*/\1/')
	expected_out="example$number.out"
	if diff <(./bp_main "$trc_file") "$expected_out" > /dev/null; then
		echo "$trc_file: ok $expected_out"
	else
		echo "$trc_file: not ok $expected_out"
	fi
done
