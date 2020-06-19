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
/**
 * This module provides a few basic statistical functions.
 */
#ifndef H_OPTSEARCH_STATS_
#define H_OPTSEARCH_STATS_

#include "common.h"
#include <math.h>

double mean(int n, double * values);

double standard_deviation(int n, double * values);

/** To avoid recalculating the mean if you already calculated it */
double standard_deviation_with_mean(int n, double * values, double mean);

/**
 * Calculates a percentage of the sum of the supplied values.
 *
 * @param percent the percentage to calculate
 * @oaram num_values the number of values
 * @param values the array containing the values
 *
 * @return the percentage value
 */
double percent_of_values(double percent, int num_values, double * values);

#endif				/* include guard H_OPTSEARCH_STATS_ */
