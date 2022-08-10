// Copyright (c) 2013-2022  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/sitter
// contact@m2osw.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


// self
//
#include    "disk.h"

#include    "names.h"


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/gethostname.h>
#include    <snapdev/mounts.h>
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// C++
//
#include    <regex>


// C
//
#include    <signal.h>
#include    <sys/statvfs.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace disk
{

SERVERPLUGINS_START(disk, 1, 0)
    , ::serverplugins::description(
            "Check disk space of all mounted drives.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("disk")
SERVERPLUGINS_END(disk)





namespace
{


std::vector<std::string> const g_ignore_filled_partitions =
{
    "^/snap/core/"
};



/** \brief The alarm handler we use to create a statvfs_try() function.
 *
 * This function is a hanlder we use to sound the alarm and prevent
 * the statvfs() from holding us up forever.
 */
void statvfs_alarm_handler(int sig)
{
    snapdev::NOT_USED(sig);
}


/** \brief A statvfs() that times out in case a drive locks us up.
 *
 * On Feb 10, 2018, I was testing the sitter daemon and it was getting
 * stuck on statvfs(). I have keybase installed on my system and
 * it failed restarting properly. Once restarted, everything worked
 * as expected.
 *
 * The `df` command would also lock up.
 *
 * The statvfs() is therefore the culprit. This function is used
 * in order to time out if the function doesn't return in a speedy
 * enough period.
 *
 * \note
 * If the function times out, it returns -1 and sets the errno
 * to EINTR. In other cases, a different errno is set.
 *
 * \param[in] path  The name of the file to stat.
 * \param[in] s  The structure were the results are saved on success.
 * \param[in] seconds  The number of seconds to wait before we time out.
 *
 * \return 0 on success, -1 on error and errno set appropriately.
 */
int statvfs_try(char const * path, struct statvfs * s, unsigned int seconds)
{
    struct sigaction alarm_action = {};
    struct sigaction saved_action = {};

    // note that the flags do not include SA_RESTART, so
    // statvfs() should be interrupted on the SIGALRM signal
    // and not restarted
    //
    alarm_action.sa_flags = 0;
    sigemptyset(&alarm_action.sa_mask);
    alarm_action.sa_handler = statvfs_alarm_handler;

    // first we setup the alarm handler as setting the alarm before
    // would mean that we don't get our handler called
    //
    if(sigaction(SIGALRM, &alarm_action, &saved_action) != 0)
    {
        return -1;
    }

    // alarm() does not return an errors
    //
    unsigned int old_alarm(alarm(seconds));
    time_t const start_time(time(nullptr));

    // clear errno
    //
    errno = 0;

    // do the statvfs() now
    //
    int const rc(statvfs(path, s));

    // save the errno value as alarm() and sigaction() might change it
    //
    int const saved_errno(errno);

    // make sure our or someone else handler does not get called
    // (this is if the alarm did not happen)
    //
    alarm(0);

    // reset the signal handler
    //
    // we have to ignore the error in this case because there is
    // pretty much nothing we can do about it (throw?!)
    //
    snapdev::NOT_USED(sigaction(SIGALRM, &saved_action, nullptr));

    // reset the alarm if required (if 0, avoid the system call)
    //
    if(old_alarm != 0)
    {
        // adjust the number of seconds with the number of seconds
        // that elapsed since we started our own alarm
        //
        time_t const elapsed(time(nullptr) - start_time);
        if(static_cast<unsigned int>(elapsed) >= old_alarm)
        {
            // we use this condition in part because the old_alarm
            // variable is an 'unsigned int'
            //
            old_alarm = 1;
        }
        else
        {
            old_alarm -= elapsed;
        }
        alarm(old_alarm);
    }

    // restore the errno that statvfs() generated
    //
    errno = saved_errno;

    // the statvfs() return code
    //
    return rc;
}


}
// no-name namespace





/** \brief Initialize disk.
 *
 * This function terminates the initialization of the disk plugin
 * by registering for different events.
 */
void disk::bootstrap()
{
    SERVERPLUGINS_LISTEN(disk, "server", server, process_watch, boost::placeholders::_1);
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * \param[in] json  The document where the results are collected.
 */
void disk::on_process_watch(as2js::JSON::JSONValueRef & json)
{
    SNAP_LOG_DEBUG
        << "disk::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    as2js::JSON::JSONValueRef e(json["disk"]);

    // read the various mounts on this server
    //
    // TBD: instead of all mounts, we may want to look into definitions
    //      in our configuration file?
    snapdev::mounts m("/proc/mounts");

    // check each disk
    size_t const max_mounts(m.size());
    for(size_t idx(0); idx < max_mounts; ++idx)
    {
        struct statvfs s;
        memset(&s, 0, sizeof(s));
        if(statvfs_try(m[idx].get_dir().c_str(), &s, 3) == 0)
        {
            // got an entry, however, we ignore entries that have a number
            // of block equal to zero because those are virtual drives
            //
            if(s.f_blocks != 0)
            {
                as2js::JSON::JSONValueRef p(e["partition"][-1]);

                // directory where this partition is attached
                //
                std::string dir(m[idx].get_dir());
                p["dir"] = dir;

                // we do not expect to get a server with blocks of 512 bytes
                // otherwise the following lose one bit of precision...
                //
                p["blocks"] =       s.f_blocks * s.f_frsize / 1024;
                p["bfree"] =        s.f_bfree  * s.f_frsize / 1024;
                p["available"] =    s.f_bavail * s.f_frsize / 1024;
                p["ffree"] =        s.f_ffree;
                p["favailable"] =   s.f_favail;
                p["flags"] =        s.f_flag;

                // is that partition full at 90% or more?
                //
                double const usage(1.0 - static_cast<double>(s.f_bavail) / static_cast<double>(s.f_blocks));
                if(usage >= 0.9)
                {
                    // if we find it in the list of partitions to
                    // ignore then we skip the full error generation
                    //
                    auto it1(std::find_if(
                              g_ignore_filled_partitions.begin()
                            , g_ignore_filled_partitions.end()
                            , [dir](auto pattern)
                            {
                                std::regex const pat(pattern);
                                return std::regex_match(dir, pat, std::regex_constants::match_any);
                            }));
                    bool const ignore(it1 != g_ignore_filled_partitions.end());

                    // we mark the partition as quite full even if the user
                    // marks it as "ignore that one"
                    //
                    p["error"] = ignore
                                    ? "partition used over 90% (ignore)"
                                    : "partition used over 90%";

                    if(!ignore)
                    {
                        // the user can also define a list of regex which
                        // we test now to ignore further partitions
                        //
                        std::string const disk_ignore(plugins()->get_server<sitter::server>()->get_server_parameter(g_name_disk_ignore));
                        advgetopt::string_list_t disk_ignore_patterns;
                        advgetopt::split_string(disk_ignore, disk_ignore_patterns, { ":" });
                        auto it2(std::find_if(
                                  disk_ignore_patterns.begin()
                                , disk_ignore_patterns.end()
                                , [dir](auto pattern)
                                {
                                    std::regex const pat(pattern);
                                    return std::regex_match(dir, pat, std::regex_constants::match_any);
                                }));
                        if(it2 == disk_ignore_patterns.end())
                        {
                            // get the name of the host for the error message
                            //
                            std::string hostname(snapdev::gethostname());

                            // priority increases as the disk gets filled up more
                            //
                            int const priority(usage >= 0.999
                                                    ? 100
                                                    : (usage >= 0.95
                                                        ? 80
                                                        : 55)); // [0.9, 0.95)
                            plugins()->get_server<sitter::server>()->append_error(
                                  e
                                , "disk"
                                , "partition \""
                                    + m[idx].get_dir()
                                    + "\" on \""
                                    + hostname
                                    + "\" is close to full ("
                                    + std::to_string(usage * 100.0) // TODO: create a snapdev::to_string(double ...) with appropriate formatting
                                    + "%)"
                                , priority);
                        }
                    }
                }
            }
        }
    }
}



} // namespace disk
} // namespace sitter
// vim: ts=4 sw=4 et
