BEGIN {
	count = 0
	sum_x = 0
	sum_y = 0
	sum_xx = 0
	sum_xy = 0
}

NF == 2 {
	if ($1 <= 0 || $2 <= 0) {
		exit 2
	}
	x = log($1)
	y = log($2)
	count++
	sum_x += x
	sum_y += y
	sum_xx += x * x
	sum_xy += x * y
	next
}

{
	exit 2
}

END {
	denominator = count * sum_xx - sum_x * sum_x
	if (count < 2 || denominator == 0) {
		exit 2
	}
	printf "%.6f\n", (count * sum_xy - sum_x * sum_y) / denominator
}
