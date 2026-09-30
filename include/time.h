/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file time.h
 * @brief Types and functions for handling time and date.
 * @Created: 2026/09/28 06:40:49 by Moutig
 * @Updated: 2026/09/30 09:20:14 by Moutig
 *
 * This header declares the ISO C and POSIX time API: calendar
 * time (struct tm), clocks, POSIX timers, and the functions that
 * convert between the various representations.
 *
 * Types and macros shared with other headers come from
 * <bits/time.h>: struct timespec, struct timeval, CLOCK_*,
 * CLOCKS_PER_SEC, TIMER_ABSTIME, TIME_UTC. The scalar types
 * time_t, clock_t, size_t, clockid_t, timer_t, and pid_t come
 * from <bits/types.h>.
 */

#ifndef _HAJ_TIME_H
# define _HAJ_TIME_H

# include <stddef.h>		/* NULL, size_t */
# include <bits/types.h>	/* time_t, clock_t, clockid_t, timer_t, pid_t */
# include <bits/time.h>		/* struct timespec, CLOCK_*, CLOCKS_PER_SEC, ... */

# ifdef __cplusplus
extern "C" {
# endif

/* ----- Forward declarations ----- */

/*
 * The tag sigevent is declared here as an incomplete type. Its
 * full definition is provided by <signal.h>. Declaring it here
 * is the minimum required by POSIX so that the timer_* prototypes
 * compile without forcing every user of <time.h> to include
 * <signal.h>.
 */

/**
 * @brief Structure for specifying asynchronous notifications.
 *
 * This structure is used to specify the type of asynchronous
 * notification to be generated when a timer expires.
 */
struct sigevent;


/* ----- Types ----- */

# ifndef _HAJ_STRUCT_TM_DEFINED
#  define _HAJ_STRUCT_TM_DEFINED
/**
 * @brief Calendar time broken down into its components.
 *
 * Set by gmtime(), localtime(), mktime(), and strptime().
 *
 * tm_gmtoff and tm_zone are new in POSIX.1-2024 (Issue 8). They
 * give the offset from UTC in seconds and the timezone
 * abbreviation respectively. They are only meaningful after a
 * call to localtime() or localtime_r(); gmtime() sets them to 0
 * and "GMT".
 */
struct tm {
	int		tm_sec;		/**< Seconds [0, 60] (60 for leap second). */
	int		tm_min;		/**< Minutes [0, 59]. */
	int		tm_hour;	/**< Hours [0, 23]. */
	int		tm_mday;	/**< Day of month [1, 31]. */
	int		tm_mon;		/**< Month of year [0, 11]. */
	int		tm_year;	/**< Years since 1900. */
	int		tm_wday;	/**< Day of week [0, 6], Sunday = 0. */
	int		tm_yday;	/**< Day of year [0, 365]. */
	int		tm_isdst;	/**< DST flag: >0 in effect, 0 not, <0 unknown. */
	long		tm_gmtoff;	/**< Seconds east of UTC (POSIX.1-2024). */
	const char	*tm_zone;	/**< Timezone abbreviation (POSIX.1-2024). */
};
# endif

# ifndef _HAJ_STRUCT_ITIMERSPEC_DEFINED
#  define _HAJ_STRUCT_ITIMERSPEC_DEFINED
/**
 * @brief Timer specification for timer_settime() and timer_gettime().
 *
 * it_value is the initial expiration (or, when read back by
 * timer_gettime(), the remaining time). it_interval is the
 * period for a repeating timer; a zero it_interval makes the
 * timer one-shot.
 */
struct itimerspec {
	struct timespec	it_interval;	/**< Timer period. */
	struct timespec	it_value;		/**< Timer expiration. */
};
# endif





/* ----- Functions ----- */

/* ----- Clock functions ----- */

/**
 * @brief Return the processor time used by the process.
 *
 * @return CPU time in ticks of CLOCKS_PER_SEC, or (clock_t)-1 if
 *         it is not available.
 */
clock_t clock(void);

/**
 * @brief Return the resolution of a clock.
 *
 * @param clock_id Clock identifier (CLOCK_REALTIME, ...).
 * @param res      Output: the resolution.
 * @return 0 on success, -1 on error with errno set.
 */
int clock_getres(clockid_t clock_id, struct timespec *res);

/**
 * @brief Read the current value of a clock.
 *
 * @param clock_id Clock identifier.
 * @param tp       Output: the current time.
 * @return 0 on success, -1 on error with errno set.
 */
int clock_gettime(clockid_t clock_id, struct timespec *tp);

/**
 * @brief Set the value of a clock.
 *
 * Only CLOCK_REALTIME may be set; other clocks are read-only.
 *
 * @param clock_id Clock identifier.
 * @param tp       New time value.
 * @return 0 on success, -1 on error with errno set.
 */
int clock_settime(clockid_t clock_id, const struct timespec *tp);

/**
 * @brief Return the CPU-time clock ID of another process.
 *
 * @param pid      Target process. 0 means the calling process.
 * @param clock_id Output: the clock ID.
 * @return 0 on success, -1 on error with errno set.
 */
int clock_getcpuclockid(pid_t pid, clockid_t *clock_id);


/* ----- Sleep functions ----- */

/**
 * @brief Suspend execution for a given interval.
 *
 * @param rqtp Requested sleep duration.
 * @param rmtp Output: remaining time if interrupted (may be NULL).
 * @return 0 on success, -1 on error with errno set to EINTR if
 *         interrupted or EINVAL for an invalid timespec.
 */
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

/**
 * @brief Sleep until an absolute time on a given clock.
 *
 * @param clock_id Clock identifier.
 * @param flags    TIMER_ABSTIME for absolute time, 0 for relative.
 * @param rqtp     Requested duration or absolute deadline.
 * @param rmtp     Output: remaining time if interrupted (may be
 *                 NULL; must be NULL when TIMER_ABSTIME is set).
 * @return 0 on success, or a positive error number (not -1) on
 *         failure.
 */
int clock_nanosleep(clockid_t				clock_id, int flags,
					const struct timespec	*rqtp,
					struct timespec			*rmtp);


/* ----- POSIX timer functions ----- */

/**
 * @brief Create a POSIX timer.
 *
 * @param clockid Clock the timer is based on.
 * @param evp     Notification specification. NULL means
 *                SIGEV_SIGNAL with SIGALRM.
 * @param timerid Output: the new timer ID.
 * @return 0 on success, -1 on error with errno set.
 */
int timer_create(clockid_t			clockid,
				 struct sigevent	*__HAJ_RESTRICT evp,
				 timer_t			*__HAJ_RESTRICT timerid);

/**
 * @brief Delete a POSIX timer.
 *
 * @param timerid Timer to delete.
 * @return 0 on success, -1 on error with errno set.
 */
int timer_delete(timer_t timerid);

/**
 * @brief Return the overrun count of a timer.
 *
 * @param timerid Timer to query.
 * @return Overrun count on success, -1 on error with errno set.
 */
int timer_getoverrun(timer_t timerid);

/**
 * @brief Return the remaining time of a timer.
 *
 * @param timerid Timer to query.
 * @param value   Output: current remaining time and interval.
 * @return 0 on success, -1 on error with errno set.
 */
int timer_gettime(timer_t timerid, struct itimerspec *value);

/**
 * @brief Arm or disarm a POSIX timer.
 *
 * @param timerid Timer to arm or disarm.
 * @param flags   TIMER_ABSTIME for absolute time, 0 for relative.
 * @param value   New time and interval. A zero it_value disarms
 *                the timer.
 * @param ovalue  Output: previous time and interval (may be NULL).
 * @return 0 on success, -1 on error with errno set.
 */
int timer_settime(timer_t					timerid, int flags,
				  const struct itimerspec	*__HAJ_RESTRICT value,
				  struct itimerspec			*__HAJ_RESTRICT ovalue);


/* ----- Time representation conversions ----- */

/**
 * @brief Return the current calendar time.
 *
 * @param tloc If non-NULL, the result is also stored here.
 * @return The current time since the Epoch, or (time_t)-1 on
 *         error.
 */
time_t time(time_t *tloc);

/**
 * @brief Return the difference between two time_t values.
 *
 * @param time1 First time.
 * @param time0 Second time.
 * @return time1 - time0, as a double.
 */
double difftime(time_t time1, time_t time0) __HAJ_CONST;

/**
 * @brief Write the current time into a timespec.
 *
 * @param ts   Output: the current time.
 * @param base Time base; must be TIME_UTC.
 * @return base on success, 0 on error.
 */
int timespec_get(struct timespec *ts, int base);


/* ----- Calendar time conversions ----- */

/**
 * @brief Convert a time_t to UTC broken-down time.
 *
 * @param timer Time to convert.
 * @return Pointer to a static struct tm, or NULL on error.
 */
struct tm *gmtime(const time_t *timer);

/**
 * @brief Thread-safe version of gmtime().
 *
 * @param timer  Time to convert.
 * @param result Output structure.
 * @return result on success, NULL on error.
 */
struct tm *gmtime_r(const time_t *__HAJ_RESTRICT timer, struct tm *__HAJ_RESTRICT result);

/**
 * @brief Convert a time_t to local broken-down time.
 *
 * @param timer Time to convert.
 * @return Pointer to a static struct tm, or NULL on error.
 */
struct tm *localtime(const time_t *timer);

/**
 * @brief Thread-safe version of localtime().
 *
 * @param timer  Time to convert.
 * @param result Output structure.
 * @return result on success, NULL on error.
 */
struct tm *localtime_r(const time_t *__HAJ_RESTRICT timer, struct tm *__HAJ_RESTRICT result);

/**
 * @brief Convert a broken-down local time to a time_t.
 *
 * Normalizes the fields of *timeptr (for example tm_mon = 12
 * becomes tm_mon = 0 and tm_year is incremented) and computes
 * the corresponding time since the Epoch. tm_isdst selects
 * between standard time and DST when the timezone has both.
 *
 * @param timeptr Broken-down local time. Modified in place.
 * @return Time since the Epoch, or (time_t)-1 on error.
 */
time_t mktime(struct tm *timeptr);


/* ----- Formatting and parsing ----- */

/**
 * @brief Format a broken-down time according to a format string.
 *
 * @param s       Output buffer.
 * @param maxsize Size of the output buffer.
 * @param format  Format string (see strftime(3)).
 * @param timeptr Broken-down time to format.
 * @return Number of bytes written, not counting the terminating
 *         NUL, or 0 if the result would not fit.
 */
size_t strftime(char			*__HAJ_RESTRICT s, size_t maxsize,
				const char		*__HAJ_RESTRICT format,
				const struct tm	*__HAJ_RESTRICT timeptr);

/**
 * @brief Parse a string into a broken-down time.
 *
 * This function is XSI, not part of the base POSIX.
 *
 * @param buf    String to parse.
 * @param format Format string (see strptime(3)).
 * @param tm     Output structure.
 * @return Pointer to the first character not processed, or NULL
 *         on error.
 */
char *strptime(const char	*__HAJ_RESTRICT buf,
			   const char	*__HAJ_RESTRICT format,
			   struct tm	*__HAJ_RESTRICT tm);

/**
 * @brief Convert a broken-down time to a string.
 *
 * @param timeptr Broken-down time to convert.
 * @return Pointer to a static string of the form
 *         "Www Mmm dd hh:mm:ss yyyy\n".
 *
 * @deprecated Obsolescent in POSIX.1-2024 (Issue 8). Use
 *             strftime() with "%a %b %e %H:%M:%S %Y" instead.
 */
char *asctime(const struct tm *timeptr) __HAJ_DEPRECATED;

/**
 * @brief Convert a time_t to a string.
 *
 * Equivalent to asctime(localtime(timer)).
 *
 * @param timer Time to convert.
 * @return Pointer to a static string.
 *
 * @deprecated Obsolescent in POSIX.1-2024 (Issue 8). Use
 *             strftime() with localtime() instead.
 */
char *ctime(const time_t *timer) __HAJ_DEPRECATED;

/**
 * @brief Convert a string to a broken-down time.
 *
 * The accepted format is documented in getdate(1). This function
 * is XSI, not part of the base POSIX.
 *
 * @param string String to parse.
 * @return Pointer to a static struct tm, or NULL on error. On
 *         error, getdate_err holds the cause (1 to 8).
 */
struct tm *getdate(const char *string);


/* ----- Timezone ----- */

/**
 * @brief Initialize the timezone conversion state.
 *
 * Reads the TZ environment variable and prepares the internal
 * data used by localtime(), localtime_r(), and mktime(). Called
 * automatically by those functions; an explicit call is only
 * needed if you changed TZ after they were first called.
 */
void tzset(void);


/* ----- XSI variables ----- */

/**
 * @brief Non-zero if Daylight Saving Time is in effect.
 *
 * @deprecated Use tm_isdst from localtime() instead.
 */
extern int daylight;

/**
 * @brief Difference in seconds between UTC and local time.
 *
 * @deprecated Use tm_gmtoff from localtime() instead.
 */
extern long timezone;

/**
 * @brief Timezone abbreviations for standard and daylight time.
 *
 * tzname[0] is the standard time abbreviation, tzname[1] the
 * daylight time one.
 *
 * @deprecated Use tm_zone from localtime() instead.
 */
extern char *tzname[];

/**
 * @brief Error code set by getdate() on failure.
 *
 * The value ranges from 1 to 8; see getdate(3) for the meaning
 * of each value.
 */
extern int getdate_err;

# ifdef __cplusplus
}
# endif

#endif /* _HAJ_TIME_H */
