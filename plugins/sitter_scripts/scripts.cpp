// Copyright (c) 2013-2025  Made to Order Software Corp.  All Rights Reserved
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
#include    "scripts.h"

#include    "names.h"


// sitter
//
#include    <sitter/version.h>
#include    <sitter/exception.h>


// libmimemail
//
#include    <libmimemail/email.h>


// cppprocess
//
#include    <cppprocess/io_capture_pipe.h>
#include    <cppprocess/process.h>


// advgetopt
//
#include    <advgetopt/conf_file.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/enumerate.h>
#include    <snapdev/file_contents.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/not_used.h>
#include    <snapdev/trim_string.h>


// libaddr
//
#include    <libaddr/addr.h>
#include    <libaddr/iface.h>


// serverplugins
//
#include    <serverplugins/collection.h>


// last include
//
#include    <snapdev/poison.h>



namespace sitter
{
namespace scripts
{



SERVERPLUGINS_START(scripts)
    , ::serverplugins::description(
            "Check whether a set of scripts are running.")
    , ::serverplugins::dependency("server")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("custom")
    , ::serverplugins::categorization_tag("script")
SERVERPLUGINS_END(scripts)








/** \brief Initialize scripts.
 *
 * This function terminates the initialization of the scripts plugin
 * by registering for various events.
 */
void scripts::bootstrap()
{
    SERVERPLUGINS_LISTEN(scripts, server, process_watch, boost::placeholders::_1);

    sitter::server::pointer_t server(plugins()->get_server<sitter::server>());
    f_script_starter = server->get_server_parameter(g_name_scripts_starter);
    if(f_script_starter.empty())
    {
        f_script_starter = g_name_scripts_starter_default;
    }

    // setup a variable that our scripts can use to save data as they
    // see fit; especially, many scripts need to remember what they've
    // done before or maybe they don't want to run too often and use a
    // file to know when to run again
    //
    std::string scripts_output = server->get_server_parameter(g_name_scripts_output);
    if(scripts_output.empty())
    {
        scripts_output = g_name_scripts_output_default;
    }
    setenv("SITTER_SCRIPTS_OUTPUT", scripts_output.c_str(), 1);

    f_log_path = server->get_server_parameter(g_name_scripts_log_path);
    if(f_log_path.empty())
    {
        f_log_path = g_name_scripts_default_log_path;
    }
    setenv("SITTER_SCRIPTS_LOG_PATH", f_log_path.c_str(), 1);

    f_log_subfolder = server->get_server_parameter(g_name_scripts_log_subfolder);
    if(f_log_subfolder.empty())
    {
        f_log_subfolder = g_name_scripts_default_log_subfolder;
    }
    setenv("SITTER_SCRIPTS_LOG_SUBFOLDER", f_log_subfolder.c_str(), 1);

    f_scripts_output_log = f_log_path + "/" + f_log_subfolder + "/sitter-scripts.log";
    f_scripts_error_log = f_log_path + "/" + f_log_subfolder + "/sitter-scripts-errors.log";
}


/** \brief Process this sitter data.
 *
 * This function runs this plugin actual check.
 *
 * The process is to go through all the script in the sitter directory
 * and run them. If they exit with 2, then they detected a problem and we
 * send an email to the administrator. If they exit with 1, the script is
 * bogus and we send an email to the administrator. If they exit with 0,
 * no problem was discovered yet.
 *
 * The scripts are standard shell scripts. The sitter environment
 * offers additional shell commands to ease certain things that
 * are otherwise very complicated.
 *
 * The results are also saved in the JSON document.
 *
 * \param[in] json  The document where the results are collected.
 */
void scripts::on_process_watch(as2js::json::json_value_ref & json)
{
    SNAP_LOG_DEBUG
        << "scripts::on_process_watch(): processing"
        << SNAP_LOG_SEND;

    std::string const scripts_path([&]()
        {
            std::string const path(plugins()->get_server<sitter::server>()->get_server_parameter(
                                    g_name_scripts_path));
            if(path.empty())
            {
                return std::string(g_name_scripts_path_default);
            }
            return path;
        }());

    f_scripts = json["scripts"];

    snapdev::glob_to_list<std::list<std::string>> script_filenames;
    script_filenames.read_path<
          snapdev::glob_to_list_flag_t::GLOB_FLAG_NO_ESCAPE
        , snapdev::glob_to_list_flag_t::GLOB_FLAG_EMPTY>(scripts_path + "/*");
    snapdev::enumerate(
              script_filenames
            , std::bind(
                  &scripts::process_script
                , this
                , std::placeholders::_1
                , std::placeholders::_2));
}


void scripts::process_script(int index, std::string script_filename)
{
    snapdev::NOT_USED(index);

    // skip any README file
    //
    // (specifically, we install a file named sitter_README.md
    // in the folder as a placeholder with documentation)
    //
    if(script_filename.find("README") != std::string::npos)
    {
        return;
    }

    f_script_filename = script_filename;
    f_start_date = time(nullptr);

    // run the script
    //
    cppprocess::process p("sitterscript");

    // Note: scripts that do not have the execution permission set are
    //       started with /bin/sh
    //
    p.set_command(f_script_starter);
    p.add_argument(script_filename);

    cppprocess::io_capture_pipe::pointer_t output_pipe(std::make_shared<cppprocess::io_capture_pipe>());
    p.set_output_io(output_pipe);

    cppprocess::io_capture_pipe::pointer_t error_pipe(std::make_shared<cppprocess::io_capture_pipe>());
    p.set_error_io(error_pipe);

    int exit_code(p.start());
    if(exit_code == 0)
    {
        exit_code = p.wait();
    }

    as2js::json::json_value_ref e(f_scripts["script"]);

    e["name"] = script_filename;
    e["exit_code"] = exit_code;

    SNAP_LOG_DEBUG
        << "script \""
        << script_filename
        << "\" exited with "
        << exit_code
        << '.'
        << SNAP_LOG_SEND;

    //if(exit_code == 0
    //&& !f_error.empty()) -- how do we know data was writte to the erro file? (with stat() + mtime?)
    //{
    //    SNAP_LOG_WARNING
    //        << "we got errors but the process exit code is 0"
    //        << SNAP_LOG_SEND;
    //}

    std::stringstream ss;
    ss << '\n' << index << "> " << script_filename << '\n';
    std::string intro(ss.str());

    // if we received output or on a failing script,
    // email the administrator
    //
    std::string output(output_pipe->get_output());
    if(exit_code != 0
    && !output.empty())
    {
        output = generate_header("OUTPUT") + output;
        if(output.back() != '\n')
        {
            output += '\n';
        }
        e["output"] = output;

        snapdev::file_contents output_file(f_scripts_output_log);
        output_file.contents(output);
        output_file.write_all();

        plugins()->get_server<sitter::server>()->append_error(
                  e
                , "scripts"
                , output
                , exit_code == 0 ? 35 : 65);
    }

    std::string error(error_pipe->get_output());
    if(!error.empty())
    {
        error = generate_header("ERROR") + error;
        if(error.back() != '\n')
        {
            error += '\n';
        }

        snapdev::file_contents error_file(f_scripts_error_log);
        error_file.contents(error);
        error_file.write_all();

        plugins()->get_server<sitter::server>()->append_error(
                  e
                , "scripts"
                , error
                , 90);
    }
}


/** \brief Generate the output or error message header.
 *
 * The function generates an email like header for the output or
 * error message. The header includes information aboutwhen the
 * output was generated, which script it is wrong, which
 * version of the sitter daemon it comes from and an IP address.
 *
 * \param[in] type  The type of header (Output or Error)
 *
 * \return The header as a string.
 */
std::string scripts::generate_header(std::string const & type)
{
    std::string header =
          std::string("--- ") + type + " -----------------------------------------------------------\n"
          "Sitter-Version: " SITTER_VERSION_STRING "\n"
          "Output-Type: " + type + "\n"
          "Date: " + format_date(f_start_date) + "\n"
          "Script: " + f_script_filename + "\n";

    // TODO: see whether we should instead use snapdev::get_hostname()
    snapdev::file_contents hostname("/etc/hostname");
    if(hostname.read_all())
    {
        header += "Hostname: ";
        header += snapdev::trim_string(hostname.contents());
        header += "\n";
    }

    // if we have a properly installed communicatord use that IP
    //
    advgetopt::conf_file_setup setup("/etc/communicatord/communicatord.conf");
    advgetopt::conf_file::pointer_t config(advgetopt::conf_file::get_conf_file(setup));
    std::string const my_ip(config->get_parameter("my_address"));
    if(!my_ip.empty())
    {
        header += "IP-Address: ";
        header += my_ip;
        header += "\n";
    }
    else
    {
        // no communicatord defined "my_address", then show
        // all the IPs on this computer
        //
        addr::iface::pointer_vector_t const ips(addr::iface::get_local_addresses());
        if(ips != nullptr)
        {
            header += "IP-Addresses: ";
            std::string sep;
            for(auto const & i : *ips)
            {
                header += sep;
                header += i->get_address().to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS);
                sep = ", ";
            }
            header += '\n';
        }
    }

    header += '\n';

    return header;
}


//bool scripts::output_available(cppprocess::io * io, cppprocess::done_reason_t reason)
//{
//    // ignore if empty (it should not happen but our code depends on that premise.)
//    //
//    if(output.isEmpty())
//    {
//        return true;
//    }
//
//    // generate a line to seperate each script entry
//    //
//    QString header;
//    if(f_new_output_script)
//    {
//        header = generate_header("OUTPUT");
//
//        // we can immediately add it to the output buffer
//        //
//        f_output += header;
//    }
//    f_output += QString::fromUtf8(output);
//
//    // if there is an output file, write that output data to it
//    //
//    if(f_output_file != nullptr)
//    {
//        // first write for this script? then write its name
//        //
//        if(f_new_output_script)
//        {
//            f_output_file->write(header.toUtf8());
//        }
//        f_output_file->write(output);
//
//        // save the last byte so we know whether we had a "\n"
//        //
//        f_last_output_byte = output.at(output.length() - 1);
//    }
//
//    f_new_output_script = false;
//
//    return true;
//}


//bool scripts::error_available(cppprocess::io * io, cppprocess::done_reason_t reason)
//{
//    // ignore if empty (it should not happen but our code depends on it.)
//    //
//    if(error.isEmpty())
//    {
//        return true;
//    }
//
//    // generate a line to seperate each script entry
//    //
//    QString header;
//    if(f_new_error_script)
//    {
//        header = generate_header("ERROR");
//
//        // we can immediately add it to the error buffer
//        //
//        f_error += header;
//    }
//    f_error += QString::fromUtf8(error);
//
//    // if there is an error output file, write that error data to it
//    //
//    if(f_error_file != nullptr)
//    {
//        // first write for this script? then write its name
//        //
//        if(f_new_error_script)
//        {
//            f_error_file->write(header.toUtf8());
//        }
//        f_error_file->write(error);
//
//        // save the last byte so we know whether we had a "\n"
//        //
//        f_last_error_byte = error.at(error.length() - 1);
//    }
//
//    f_new_error_script = false;
//
//    return true;
//}


// TODO: move that to our edhttp http_date implementation
std::string scripts::format_date(time_t const t)
{
    struct tm f;
    gmtime_r(&t, &f);
    char date[32]; // YYYY/MM/DD HH:MM:SS (19 characters)
    strftime(date, sizeof(date) - 1, "%D %T", &f);
    return date;
}



} // namespace scripts
} // namespace sitter
// vim: ts=4 sw=4 et
