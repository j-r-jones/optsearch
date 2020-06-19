/* 
 * This file is part of OptSearch.
 *
 * OptSearch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OptSearch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OptSearch.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2007-2018 Jessica Jones
 */

#include "stats.h"
#include <gsl/gsl_statistics.h>

/* TODO For now, this will be a wrapper around the GSL functions, but it would
 * be better not to depend on GSL being available (and accurate).
 */

/**
 * Provides some basic statistical functions.  In particular, we need to
 * be able to calculate:
 *	- standard deviation
 *	- mean
 * of an array of doubles.
 */

/*
 * Watch out for overflow and you may need to consider Kahan summation too,
 * depending on your values.
 */

const int stride = 1;

double mean(int n, double * values)
{
	/* Just a wrapper for
	 * double gsl_stats_mean (const double data[], size_t stride, size_t n)
	 */
	return gsl_stats_mean(values, stride, n);
}

double standard_deviation(int n, double * values)
{
	/* Just a wrapper for
	 * gsl_stats_sd (const double data[], size_t stride, size_t n)
	 */
	return gsl_stats_sd(values, stride, n);
}

double standard_deviation_with_mean(int n, double * values, double mean)
{
	/* Just a wrapper for
	 * gsl_stats_sd (const double data[], size_t stride, size_t n)
	 */
	return gsl_stats_sd_m(values, stride, n, mean);
}

double percent_of_values(double percent, int num_values, double * values)
{
	int i;
	double sum = 0.0;

	/* TODO This could overflow, potentially, if the values are all large or
	 * there are a lot of them */
	for (i = 0; i < num_values; i++) {
		sum += values[i];
	}
	
	return (sum / 100.0) * percent;
}
