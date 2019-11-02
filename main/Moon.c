// Moon clock
// Copyright (c) 2019 Adrian Kennard, Andrews & Arnold Limited, see LICENSE file (GPL)
static const char TAG[] = "Moon";

#include "revk.h"
#include "esp_sntp.h"
#include <driver/i2c.h>
#include <math.h>
#include "oled.h"
#include CONFIG_MOON

#define settings	\
	s8(oledsda,27)	\
	s8(oledscl,14)	\
	s8(oledaddress,0x3D)	\
	u8(oledcontrast,127)	\
	b(oledflip)	\

#define u32(n,d)	uint32_t n;
#define s8(n,d)	int8_t n;
#define u8(n,d)	int8_t n;
#define b(n) uint8_t n;
#define s(n,d) char * n;
settings
#undef u32
#undef s8
#undef u8
#undef b
#undef s
#define PI      3.1415926535897932384626433832795029L
#define sinld(a)        sinl(PI*(a)/180.0L)
   time_t
fullmoon (int cycle)
{                               // report full moon for specific lunar cycle
   long double k = cycle + 0.5;
   long double T = k / 1236.85L;
   long double JD =
      2415020.75933L + 29.53058868L * k + 0.0001178L * T * T - 0.000000155L * T * T * T +
      0.00033L * sinld (166.56L + 132.87L * T - 0.009173L * T * T);
   long double M = 359.2242L + 29.10535608L * k - 0.0000333L * T * T - 0.00000347L * T * T * T;
   long double M1 = 306.0253L + 385.81691806L * k + 0.0107306L * T * T + 0.00001236L * T * T * T;
   long double F = 21.2964L + 390.67050646L * k - 0.0016528L * T * T - 0.00000239L * T * T * T;
   long double A = (0.1734 - 0.000393 * T) * sinld (M)  //
      + 0.0021 * sinld (2 * M)  //
      - 0.4068 * sinld (M1)     //
      + 0.0161 * sinld (2 * M1) //
      - 0.0004 * sinld (3 * M1) //
      + 0.0104 * sinld (2 * F)  //
      - 0.0051 * sinld (M + M1) //
      - 0.0074 * sinld (M - M1) //
      + 0.0004 * sinld (2 * F + M)      //
      - 0.0004 * sinld (2 * F - M)      //
      - 0.0006 * sinld (2 * F + M1)     //
      + 0.0010 * sinld (2 * F - M1)     //
      + 0.0005 * sinld (M + 2 * M1);    //
   JD += A;
   return (JD - 2440587.5L) * 86400LL;
}

int
lunarcycle (time_t t)
{                               // report cycle for previous full moon
   int cycle = ((long double) t + 2207726238UL) / 2551442.86195200L;
   time_t f = fullmoon (cycle);
   if (t < f)
      return cycle - 1;
   f = fullmoon (cycle + 1);
   if (t >= f)
      return cycle + 1;
   return cycle;
}

const char *
app_command (const char *tag, unsigned int len, const unsigned char *value)
{
   if (!strcmp (tag, "contrast"))
   {
      oled_set_contrast (atoi ((char *) value));
      return "";                // OK
   }
   if (!strcmp (tag, "time"))
   {
      if (!len)
      {
         sntp_init ();
      } else
      {
         sntp_stop ();
         struct tm t = { };
         int y = 0,
            m = 0,
            d = 0,
            H = 0,
            M = 0,
            S = 0;
         char z = 0;
         sscanf ((char *) value, "%d-%d-%d %d:%d:%d%c", &y, &m, &d, &H, &M, &S, &z);
         t.tm_year = y - 1900;
         t.tm_mon = m - 1;
         t.tm_mday = d;
         t.tm_hour = H;
         t.tm_min = M;
         t.tm_sec = S;
         if (!z)
            t.tm_isdst = -1;
         struct timeval v = { };
         v.tv_sec = mktime (&t);
         settimeofday (&v, NULL);
      }
      return "";
   }
   return NULL;
}

void
app_main ()
{
   revk_init (&app_command);
#define b(n) revk_register(#n,0,sizeof(n),&n,NULL,SETTING_BOOLEAN);
#define u32(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s8(n,d) revk_register(#n,0,sizeof(n),&n,#d,SETTING_SIGNED);
#define u8(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s(n,d) revk_register(#n,0,0,&n,d,0);
   settings
#undef u32
#undef s8
#undef u8
#undef b
#undef s
      if (oledsda >= 0 && oledscl >= 0)
      oled_start (1, oledaddress, oledscl, oledsda, oledflip);
   oled_set_contrast (oledcontrast);
   // Main task...
   while (1)
   {
      time_t now = time (0);
      struct tm t;
      localtime_r (&now, &t);
      oled_lock ();
      oled_icon (0, 0, moon, CONFIG_OLED_WIDTH, CONFIG_OLED_HEIGHT);
      if (t.tm_year > 100)
      {                         // Clock set
         int c = lunarcycle (now);
         time_t last = fullmoon (c);
         time_t next = fullmoon (c + 1);
         float phase = (float) M_PI * 2 * (now - last) / (next - last);
#define w (CONFIG_OLED_WIDTH/2)
         if (phase < M_PI)
         {                      // dim on right (northern hemisphere)
            float q = (float) w * cos (phase);
            for (int y = 0; y < w * 2; y++)
            {
               float d = (float) y + 0.5 - w;
               float v = q * sqrt (1 - (float) d / w * (float) d / w) + w;
               int l = ceil (v);
               if (l)
                  oled_set (l - 1, y, ((float) l - v) * oled_get (l - 1, y));
               for (int x = l; x < CONFIG_OLED_WIDTH; x++)
                  oled_set (x, y, (x + y) & 1 ? 0 : oled_get (x, y) >> 3);
            }
         } else
         {                      // dim on left (northern hemisphere)
            float q = -(float) w * cos (phase);
            for (int y = 0; y < w * 2; y++)
            {
               float d = (float) y + 0.5 - w;
               float v = q * sqrt (1 - (float) d / w * (float) d / w) + w;
               int r = floor (v);
               if (r < w * 2)
                  oled_set (r, y, ((float) r + 1 - v) * oled_get (r, y));
               for (int x = 0; x < r; x++)
                  oled_set (x, y, (x + y) & 1 ? 0 : oled_get (x, y) >> 3);
            }
         }
         const char *mname[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
         oled_text (1, CONFIG_OLED_WIDTH - 4 * 6, CONFIG_OLED_HEIGHT - 8 * 1, "%04d", t.tm_year + 1900);
         oled_text (1, CONFIG_OLED_WIDTH - 3 * 6, CONFIG_OLED_HEIGHT - 8 * 2, "%s", mname[t.tm_mon]);
         oled_text (1, CONFIG_OLED_WIDTH - 2 * 6, CONFIG_OLED_HEIGHT - 8 * 3, "%02d", t.tm_mday);
         oled_text (1, 0, CONFIG_OLED_HEIGHT - 8 * 1, "%02d:%02d", t.tm_hour, t.tm_min);
         int d = now - last;
         oled_text (1, 0, 8 * 1, "%02d", d / 86400);
         oled_text (1, 0, 8 * 0, "%02d:%02d", d / 3600 % 24, d / 60 % 60);
         d = next - now;
         oled_text (1, CONFIG_OLED_WIDTH - 2 * 6, 8 * 1, "%02d", d / 86400);
         oled_text (1, CONFIG_OLED_WIDTH - 4 * 6 - 2, 8 * 0, "%02d:%02d", d / 3600 % 24, d / 60 % 60);
      }
      oled_unlock ();
      sleep (1);
   }
}
