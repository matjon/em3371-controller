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

/*
 * Routines to calculate the dew point of humid air.
 *
 * Calculating the psychrometric properties of air is not straightforward.
 *
 * Here I am using Arden Buck equations as specified in
 *      Buck (1996), Buck Research CR-1A User's Manual, Appendix 1. (PDF)
 *      http://www.hygrometers.com/wp-content/uploads/CR-1A-users-manual-2009-12.pdf
 *
 * See also:
 *      https://en.wikipedia.org/wiki/Dew_point_temperature#Calculating_the_dew_point
 *      https://en.wikipedia.org/wiki/Arden_Buck_equation
 *
 * Units used:
 *      temperature in °C,
 *      relative humidity in % (e.g. the humidity of 50% is processed as 50, not 0.50)
 *
 *
 * To obtain the dew point from temperature (T) and relative humidity (RH), we have first to
 * calculate the saturation vapor pressure (es) by
 *
 *      es = f1(T)
 *
 * then we can get vapor pressure (e) from the formula
 *
 *      e = es * RH / 100
 *
 * and from it calculate the dew point (DP) by:
 *      DP = f2(e)
 *
 *
 * To generalize, I am going to use the variable (a) in place of either (ai) or (aw), etc.
 *
 * Let us define
 *
 *      Z(T) = (b - T/d) * T / (T + c)
 *
 * and with it we have f1(T) = EF * a * exp(Z)
 *
 * The function f2 uses the value (s):
 *
 *      s = ln(e/EF) - ln(a) = ln( e / (EF * a))
 *
 * where (a) is either (ai) or (aw).
 *
 * By substituting the formula for (e) we get:
 *
 *      s = ln( es * RH / (100 * EF * a))
 *      s = ln( f1(T) * RH / (100 * EF * a))
 *
 * and by expanding f1(T):
 *
 *      s = ln( EF * a * exp(Z) * RH / (100 * EF * a))
 *      s = ln(exp(Z) * RH / 100)
 *      s = ln(RH / 100) + Z
 *
 *
 * For temperatures in the range (-20°C, 30°C):
 *
 * Z(T) = (b - T/d) * T / (T + c)
 */

#include <math.h>
#include <stdio.h>

static inline double sqr(double x)
{
        return x * x;
}

#define VAR_Z(b,c,d, T) ( (b - T/d) * T/(T + c) )

#define VAR_S(b,c,d, T,RH)  ( log(RH / 100.0) + VAR_Z (b,c,d, T) )

#define VAR_F2_WITH_S(b,c,d, S)                                                 \
        (d / 2.0 * (b - S - sqrt( sqr(b-S) - 4 * c * S / d)))

#define VAR_F2_FUN(FUNNAME, b,c,d)                                              \
        static inline double FUNNAME(double temperature, double humidity)       \
        {                                                                       \
                double s = VAR_S(b,c,d, temperature, humidity);                 \
                return VAR_F2_WITH_S(b,c,d, s);                                 \
        }

VAR_F2_FUN(dew_point_on_water, 18.678, 257.14, 234.5)
VAR_F2_FUN(dew_point_on_ice, 23.036, 279.82, 333.7)

double dew_point(double dry_bulb_temperature, double rel_humidity)
{
        if (rel_humidity == 0) {
                fputs("Tried to calculate dew point with "
                        "relative humidity of 0%, which is incorrect.\n", stderr);
                return nan("");
        }

        // TODO: consider checking for abnormal temperature and humidity > 100%.

        if (dry_bulb_temperature >= 0.0) {
                return dew_point_on_water(dry_bulb_temperature, rel_humidity);
        } else {
                return dew_point_on_ice(dry_bulb_temperature, rel_humidity);
        }
}
