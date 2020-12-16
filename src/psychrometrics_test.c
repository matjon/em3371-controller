/*
 *  Copyright (C) 2020 Mateusz Jończyk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "psychrometrics.h"
#include <math.h>
#include <stdio.h>
#include <sys/time.h>

void print_dew_point(double temperature, double humidity, double reference)
{
        fprintf(stdout, "%.2f °C, %.2f %% humidity => dew point %.2f (reference: %.1f)\n",
                temperature, humidity, dew_point(temperature, humidity),
                reference);
}

double fahrenheit_to_celcius(double temperature)
{
        return (temperature - 32.0 ) * 5.0 / 9.0;
}

void dew_point_calculation_benchmarks()
{
        double sum = 0;
        int calculations_done = 0;
        struct timeval start;
        struct timeval end;

        fprintf(stderr, "Starting a benchmark\n");

        gettimeofday(&start, NULL);

        for (int i = 0; i < 100; i++) {
                // We can't calculate dew point for 0% relative humidity
                for (int j = 1; j <= 100; j++) {
                        for (int k = 0; k < 100; k++) {
                                sum += dew_point(i, j);
                                calculations_done++;
                        }
                }
        }

        gettimeofday(&end, NULL);

        double total_time = (end.tv_sec - start.tv_sec)
                        + (double)(end.tv_usec - start.tv_usec) / 1000000.0;

        fprintf(stderr, "Performed %d dew point calculations in %f s\n",
                        calculations_done, total_time);
        fprintf(stderr, "Sum = %f\n", sum);
}

int main()
{
        fprintf(stdout, "Small differences are normal as there is some variability in formulas\n");
        fprintf(stdout, "especially at temperatures below 0°C.\n");
        // and values are rounded to 1°F

        fprintf(stdout, "Reference values from diagram, less reliable:\n");
        // Reference values are from chart at
        //      https://upload.wikimedia.org/wikipedia/commons/9/9d/PsychrometricChart.SeaLevel.SI.svg
        print_dew_point(30, 60, 21.5);
        print_dew_point(0, 80, -5);
        print_dew_point(-5, 70, -10);


        fprintf(stdout, "Reference values from table:\n");
        // Values from
        //      https://www.nwcg.gov/publications/pms437/weather/temp-rh-dp-tables#TOC-Dry-Bulb-Temp-41-60
        print_dew_point(fahrenheit_to_celcius(36), 38, fahrenheit_to_celcius(13));
        print_dew_point(fahrenheit_to_celcius(31), 89, fahrenheit_to_celcius(28));
        print_dew_point(fahrenheit_to_celcius(31), 18, fahrenheit_to_celcius(-8));

        print_dew_point(fahrenheit_to_celcius(32), 11, fahrenheit_to_celcius(-16));
        print_dew_point(fahrenheit_to_celcius(32), 89, fahrenheit_to_celcius(29));
        print_dew_point(fahrenheit_to_celcius(56), 2, fahrenheit_to_celcius(-29));
        print_dew_point(fahrenheit_to_celcius(56), 50, fahrenheit_to_celcius(37));
        print_dew_point(fahrenheit_to_celcius(56), 94, fahrenheit_to_celcius(54));
        print_dew_point(fahrenheit_to_celcius(69), 2, fahrenheit_to_celcius(-26));
        print_dew_point(fahrenheit_to_celcius(69), 39, fahrenheit_to_celcius(43));
        print_dew_point(fahrenheit_to_celcius(69), 63, fahrenheit_to_celcius(56));
        print_dew_point(fahrenheit_to_celcius(69), 90, fahrenheit_to_celcius(66));


        print_dew_point(fahrenheit_to_celcius(93), 4, fahrenheit_to_celcius(9));
        print_dew_point(fahrenheit_to_celcius(93), 31, fahrenheit_to_celcius(58));
        print_dew_point(fahrenheit_to_celcius(93), 69, fahrenheit_to_celcius(81));
        print_dew_point(fahrenheit_to_celcius(93), 93, fahrenheit_to_celcius(91));

        dew_point_calculation_benchmarks();

        fprintf(stdout, "Should display \"nan\" and possibly an error message\n");

        print_dew_point( 10, 0, nan(""));

        return 0;
}
