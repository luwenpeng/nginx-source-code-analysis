
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>


/*
 * The time may be updated by signal handler or by several threads.
 *
 * 时间可以由信号处理函数或者一些线程更新.
 *
 * The time update operations are rare and require to hold the ngx_time_lock.
 *
 * 时间更新操作是比较少见的, 需要抢占 ngx_time_lock.
 *
 * The time read operations are frequent, so they are lock-free and get time
 * values and strings from the current slot.  Thus thread may get the corrupted
 * values only if it is preempted while copying and then it is not scheduled
 * to run more than NGX_TIME_SLOTS seconds.
 *
 * 时间读取操作频繁, 因此它们是无锁的, 并从当前插槽中获取时间值和字符串.
 * 只有当线程复制时被抢占且未按计划运行超过 NGX_TIME_SLOTS 秒时会得到损耗的时间值.
 */

#define NGX_TIME_SLOTS   64

static ngx_uint_t        slot;
static ngx_atomic_t      ngx_time_lock;

volatile ngx_msec_t      ngx_current_msec;

// 时间缓存指针
volatile ngx_time_t     *ngx_cached_time;
volatile ngx_str_t       ngx_cached_err_log_time;
volatile ngx_str_t       ngx_cached_http_time;
volatile ngx_str_t       ngx_cached_http_log_time;
volatile ngx_str_t       ngx_cached_http_log_iso8601;
volatile ngx_str_t       ngx_cached_syslog_time;

#if !(NGX_WIN32)

/*
 * localtime() and localtime_r() are not Async-Signal-Safe functions, therefore,
 * they must not be called by a signal handler, so we use the cached
 * GMT offset value. Fortunately the value is changed only two times a year.
 *
 * localtime() 和 localtime_r() 不是异步信号安全的函数, 所以他们不能被信号处理
 * 函数调用, 因此我们使用缓存的 GMT 偏移值(这个值一年只改变两次).
 */

static ngx_int_t         cached_gmtoff;
#endif

// 轮换周期为 64 的时间槽
static ngx_time_t        cached_time[NGX_TIME_SLOTS];
static u_char            cached_err_log_time[NGX_TIME_SLOTS]
                                    [sizeof("1970/09/28 12:00:00")];
static u_char            cached_http_time[NGX_TIME_SLOTS]
                                    [sizeof("Mon, 28 Sep 1970 06:00:00 GMT")];
static u_char            cached_http_log_time[NGX_TIME_SLOTS]
                                    [sizeof("28/Sep/1970:12:00:00 +0600")];
static u_char            cached_http_log_iso8601[NGX_TIME_SLOTS]
                                    [sizeof("1970-09-28T12:00:00+06:00")];
static u_char            cached_syslog_time[NGX_TIME_SLOTS]
                                    [sizeof("Sep 28 12:00:00")];


static char  *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char  *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/*
 * brief  : 初始化时间缓存.
 */
void
ngx_time_init(void)
{
    ngx_cached_err_log_time.len = sizeof("1970/09/28 12:00:00") - 1;
    ngx_cached_http_time.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
    ngx_cached_http_log_time.len = sizeof("28/Sep/1970:12:00:00 +0600") - 1;
    ngx_cached_http_log_iso8601.len = sizeof("1970-09-28T12:00:00+06:00") - 1;
    ngx_cached_syslog_time.len = sizeof("Sep 28 12:00:00") - 1;

    ngx_cached_time = &cached_time[0];

    ngx_time_update();
}


/*
 * brief  : 更新时间缓存.
 */
void
ngx_time_update(void)
{
    u_char          *p0, *p1, *p2, *p3, *p4;
    ngx_tm_t         tm, gmt;
    time_t           sec;
    ngx_uint_t       msec;
    ngx_time_t      *tp;
    struct timeval   tv;

    // 获取原子锁
    if (!ngx_trylock(&ngx_time_lock)) {
        return;
    }

    // 调用 gettimeofday
    ngx_gettimeofday(&tv);

    sec = tv.tv_sec; // 秒
    msec = tv.tv_usec / 1000; // 毫秒

    ngx_current_msec = (ngx_msec_t) sec * 1000 + msec; // 毫秒

    tp = &cached_time[slot];

    /*
     * 同 1 秒内仅更新 ngx_cached_time
     * 不重复更新下列时间缓存:
     * ngx_cached_http_time
     * ngx_cached_err_log_time
     * ngx_cached_http_log_time
     * ngx_cached_http_log_iso8601
     * ngx_cached_syslog_time
     */
    if (tp->sec == sec) {
        tp->msec = msec;
        ngx_unlock(&ngx_time_lock);
        return;
    }

    // 时间轮
    if (slot == NGX_TIME_SLOTS - 1) {
        slot = 0;
    } else {
        slot++;
    }

    tp = &cached_time[slot];

    tp->sec = sec;
    tp->msec = msec;

    ngx_gmtime(sec, &gmt);


    // ngx_cached_http_time 用在 HTTP headers: "Mon, 28 Sep 1970 06:00:00 GMT"
    p0 = &cached_http_time[slot][0];

    (void) ngx_sprintf(p0, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                       week[gmt.ngx_tm_wday], gmt.ngx_tm_mday,
                       months[gmt.ngx_tm_mon - 1], gmt.ngx_tm_year,
                       gmt.ngx_tm_hour, gmt.ngx_tm_min, gmt.ngx_tm_sec);

    // 获取 GMT 偏移值
#if (NGX_HAVE_GETTIMEZONE)

    tp->gmtoff = ngx_gettimezone();
    ngx_gmtime(sec + tp->gmtoff * 60, &tm);

#elif (NGX_HAVE_GMTOFF)

    ngx_localtime(sec, &tm);
    cached_gmtoff = (ngx_int_t) (tm.ngx_tm_gmtoff / 60);
    tp->gmtoff = cached_gmtoff;

#else

    ngx_localtime(sec, &tm);
    cached_gmtoff = ngx_timezone(tm.ngx_tm_isdst);
    tp->gmtoff = cached_gmtoff;

#endif


    // ngx_cached_err_log_time 用在 error log: "1970/09/28 12:00:00"
    p1 = &cached_err_log_time[slot][0];

    (void) ngx_sprintf(p1, "%4d/%02d/%02d %02d:%02d:%02d",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec);


    // ngx_cached_http_log_time 用在 HTTP access log: "28/Sep/1970:12:00:00 +0600"
    p2 = &cached_http_log_time[slot][0];

    (void) ngx_sprintf(p2, "%02d/%s/%d:%02d:%02d:%02d %c%02i%02i",
                       tm.ngx_tm_mday, months[tm.ngx_tm_mon - 1],
                       tm.ngx_tm_year, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec,
                       tp->gmtoff < 0 ? '-' : '+',
                       ngx_abs(tp->gmtoff / 60), ngx_abs(tp->gmtoff % 60));

    // ngx_cached_http_log_iso8601 ISO 8601 标准格式: "1970-09-28T12:00:00+06:00"
    p3 = &cached_http_log_iso8601[slot][0];

    (void) ngx_sprintf(p3, "%4d-%02d-%02dT%02d:%02d:%02d%c%02i:%02i",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec,
                       tp->gmtoff < 0 ? '-' : '+',
                       ngx_abs(tp->gmtoff / 60), ngx_abs(tp->gmtoff % 60));

    // ngx_cached_syslog_time 用在 syslog: "Sep 28 12:00:00"
    p4 = &cached_syslog_time[slot][0];

    (void) ngx_sprintf(p4, "%s %2d %02d:%02d:%02d",
                       months[tm.ngx_tm_mon - 1], tm.ngx_tm_mday,
                       tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);

    // 内存屏障, 对我们的指令进行排序, 强制 cpu 执行完前面的指令后再执行后面的指令
    ngx_memory_barrier();

    // 修改时间缓存的指针
    ngx_cached_time = tp;
    ngx_cached_http_time.data = p0;
    ngx_cached_err_log_time.data = p1;
    ngx_cached_http_log_time.data = p2;
    ngx_cached_http_log_iso8601.data = p3;
    ngx_cached_syslog_time.data = p4;

    // 释放原子锁
    ngx_unlock(&ngx_time_lock);
}


/*
 * brief  : 信号安全的时间更新操作.
 *          仅更新了 err_log 与 sys_log 的时间缓存.
 */
#if !(NGX_WIN32)

void
ngx_time_sigsafe_update(void)
{
    u_char          *p, *p2;
    ngx_tm_t         tm;
    time_t           sec;
    ngx_time_t      *tp;
    struct timeval   tv;

    if (!ngx_trylock(&ngx_time_lock)) {
        return;
    }

    ngx_gettimeofday(&tv);

    sec = tv.tv_sec;

    tp = &cached_time[slot];

    // 忽略毫秒级的时间更新
    if (tp->sec == sec) {
        ngx_unlock(&ngx_time_lock);
        return;
    }

    if (slot == NGX_TIME_SLOTS - 1) {
        slot = 0;
    } else {
        slot++;
    }

    tp = &cached_time[slot];

    /* 本函数中仅更新了 err_log 与 sys_log 的时间缓存.
     * 未更新 ngx_cached_time 的值.
     * 此时 ngx_cached_time 仍然为 &cached_time[slot - 1] 的值
     * 此处强制将下一个 cached_time[slot] 的 tp->sec 设置为 0
     * 待下一次执行下列代码时 if 判断会返回 false, 强制更新时间缓存.
     * tp = &cached_time[slot];
     * if (tp->sec == sec)
     * {
     * }
     */
    tp->sec = 0;

    ngx_gmtime(sec + cached_gmtoff * 60, &tm);

    // 仅更新了 err_log 与 sys_log 的时间缓存
    p = &cached_err_log_time[slot][0];

    (void) ngx_sprintf(p, "%4d/%02d/%02d %02d:%02d:%02d",
                       tm.ngx_tm_year, tm.ngx_tm_mon,
                       tm.ngx_tm_mday, tm.ngx_tm_hour,
                       tm.ngx_tm_min, tm.ngx_tm_sec);

    p2 = &cached_syslog_time[slot][0];

    (void) ngx_sprintf(p2, "%s %2d %02d:%02d:%02d",
                       months[tm.ngx_tm_mon - 1], tm.ngx_tm_mday,
                       tm.ngx_tm_hour, tm.ngx_tm_min, tm.ngx_tm_sec);

    ngx_memory_barrier();

    ngx_cached_err_log_time.data = p;
    ngx_cached_syslog_time.data = p2;

    ngx_unlock(&ngx_time_lock);
}

#endif


/*
 * brief  : 将 time_t 格式的时间 t 转换成 "%02d %s %4d %02d:%02d:%02d GMT"
 *          格式, 写入 buf.
 * param  : [in/out] buf
 * param  : [in] t
 * return : buf + writelen
 */
u_char *
ngx_http_time(u_char *buf, time_t t)
{
    ngx_tm_t  tm;

    ngx_gmtime(t, &tm);

    return ngx_sprintf(buf, "%s, %02d %s %4d %02d:%02d:%02d GMT",
                       week[tm.ngx_tm_wday],
                       tm.ngx_tm_mday,
                       months[tm.ngx_tm_mon - 1],
                       tm.ngx_tm_year,
                       tm.ngx_tm_hour,
                       tm.ngx_tm_min,
                       tm.ngx_tm_sec);
}


/*
 * brief  : 使用 time_t 生成 cookie 中的过期时间.
 * param  : [in/out] buf
 * param  : [in] t
 * return : buf + writelen
 */
u_char *
ngx_http_cookie_time(u_char *buf, time_t t)
{
    ngx_tm_t  tm;

    ngx_gmtime(t, &tm);

    /*
     * Netscape 3.x does not understand 4-digit years at all and
     * 2-digit years more than "37"
     */

    return ngx_sprintf(buf,
                       (tm.ngx_tm_year > 2037) ?
                                         "%s, %02d-%s-%d %02d:%02d:%02d GMT":
                                         "%s, %02d-%s-%02d %02d:%02d:%02d GMT",
                       week[tm.ngx_tm_wday],
                       tm.ngx_tm_mday,
                       months[tm.ngx_tm_mon - 1],
                       (tm.ngx_tm_year > 2037) ? tm.ngx_tm_year:
                                                 tm.ngx_tm_year % 100,
                       tm.ngx_tm_hour,
                       tm.ngx_tm_min,
                       tm.ngx_tm_sec);
}


/*
 * brief  : 将时间 t 转换成 ngx_tm_t 类型的时间.
 * param  : [in] t : 转换的时间, 是格林威治时间1970年1月1日0点0分0秒到某一时间的秒数
 * param  : [in/out] tp : ngx_tm_t 类型的时间, 实际上就是标准的 struct tm 类型时间
 */
void
ngx_gmtime(time_t t, ngx_tm_t *tp)
{
    ngx_int_t   yday;
    ngx_uint_t  n, sec, min, hour, mday, mon, year, wday, days, leap;

    /* the calculation is valid for positive time_t only */

    n = (ngx_uint_t) t;

    days = n / 86400;

    /* January 1, 1970 was Thursday */

    wday = (4 + days) % 7;

    n %= 86400;
    hour = n / 3600;
    n %= 3600;
    min = n / 60;
    sec = n % 60;

    /*
     * the algorithm based on Gauss' formula,
     * see src/http/ngx_http_parse_time.c
     */

    /* days since March 1, 1 BC */
    days = days - (31 + 28) + 719527;

    /*
     * The "days" should be adjusted to 1 only, however, some March 1st's go
     * to previous year, so we adjust them to 2.  This causes also shift of the
     * last February days to next year, but we catch the case when "yday"
     * becomes negative.
     */

    year = (days + 2) * 400 / (365 * 400 + 100 - 4 + 1);

    yday = days - (365 * year + year / 4 - year / 100 + year / 400);

    if (yday < 0) {
        leap = (year % 4 == 0) && (year % 100 || (year % 400 == 0));
        yday = 365 + leap + yday;
        year--;
    }

    /*
     * The empirical formula that maps "yday" to month.
     * There are at least 10 variants, some of them are:
     *     mon = (yday + 31) * 15 / 459
     *     mon = (yday + 31) * 17 / 520
     *     mon = (yday + 31) * 20 / 612
     */

    mon = (yday + 31) * 10 / 306;

    /* the Gauss' formula that evaluates days before the month */

    mday = yday - (367 * mon / 12 - 30) + 1;

    if (yday >= 306) {

        year++;
        mon -= 10;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday -= 306;
         */

    } else {

        mon += 2;

        /*
         * there is no "yday" in Win32 SYSTEMTIME
         *
         * yday += 31 + 28 + leap;
         */
    }

    tp->ngx_tm_sec = (ngx_tm_sec_t) sec;
    tp->ngx_tm_min = (ngx_tm_min_t) min;
    tp->ngx_tm_hour = (ngx_tm_hour_t) hour;
    tp->ngx_tm_mday = (ngx_tm_mday_t) mday;
    tp->ngx_tm_mon = (ngx_tm_mon_t) mon;
    tp->ngx_tm_year = (ngx_tm_year_t) year;
    tp->ngx_tm_wday = (ngx_tm_wday_t) wday;
}


/*
 * brief  : 设置过期时间.
 * param  : when 它仅表示一天内的秒数.
 * return : -1 : 失败
 * note   : 将 when 转换成格林威治时间1970年1月1日凌晨0点O分O秒到某一时间的秒数 t.
 *          若 t > now 则返回 t
 *          若 t < now 则返回 t + 3600
 *          目前仅用于与 expires 配置项相关的缓存过期功能
 */
time_t
ngx_next_time(time_t when)
{
    time_t     now, next;
    struct tm  tm;

    // 从缓存中获取格林威治时间1970年1月1日0点0分0秒到当前时间的秒数
    now = ngx_time();

    ngx_libc_localtime(now, &tm);

    tm.tm_hour = (int) (when / 3600);
    when %= 3600;
    tm.tm_min = (int) (when / 60);
    tm.tm_sec = (int) (when % 60);

    // 将时间转换为自1970年1月1日以来持续时间的秒数, 发生错误时返回-1.
    next = mktime(&tm);

    if (next == -1) {
        return -1;
    }

    if (next - now > 0) {
        return next;
    }

    tm.tm_mday++;

    /* mktime() should normalize a date (Jan 32, etc) */

    next = mktime(&tm);

    if (next != -1) {
        return next;
    }

    return -1;
}
