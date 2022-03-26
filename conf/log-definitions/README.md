
# Log Definition File

One configuration file defines one log definition. Each definition includes
a set of variables definition parameters on how to handle the logs.

For example, we can detect that the logs are missing or that the logs
are too large and report these issues to the administrator.

## Name `name=<name>`

The name of this definition. This is used in the reports. It has no other
purpose.

Each log definition must have a different name.

## Mandatory `mandatory=<true | false>`

A boolean definition whether the log is mandatory. If missing, an error
is generated.

## Secure `secure=<true | false>`

A boolean definition whether the log is considered secure or not.

An error detected on a secure log has a higher priority.

## Path `path=<path-to-log-files>`

The path to where the log files are expected.

## Pattern `pattern=<glob pattern>`

A pattern to be used to search for log files. In most cases, it would look
like `*.log`.

## Maximum Size `max_size=<size>`

The maximum size a log file is expected to be. It is an error if the size
grows over that size.

Note that our snaplogger also has a feature that prevents it from writing
to a file if considered too large. It can also automatically call the
`logrotate` command to rotate the files.

The sizes supported by the advgetopt library are supported here. For
example, `12Mb` means twelve megabytes.

## Maximum Age `max_age=<duration>`

The maximum age represents an amount of time of how old the log file can
get. Once that age is reached, the file is expected to be deleted. This
is useful if you have a policy telling your users that you do not keep
their information more than 90 days. This test will ensure that such old
files get reported to the administrator who in turn can make sure that the
file gets deleted.

Note: the sitter actually reads the log file to determine its age. The
date found in the oldest message (a.k.a. first message) is used for this
test. If reading the log file does not produce a valid date, then the
sitter uses the `stat()` command to determine the last modification date
instead.

## Owner `owner=<name>`

Specify the name of the owner. If the log file owner is not exactly equal
to this name, then it is in error.

Often, the wrong name on a log file means that the service supposed to write
to the file will not be able to do so.

## Group `group=<name>`

Specify the name of the group. If the log file group is not equal to this
name, then it is in error.

In general an invalid group ownership is less of an issue. For a log, though,
if the group has write permissions, it could mean that some tools may not
be able to write to the file even though it should be able to.

## Mode `mode=<mode-format>`

The mode the file is expected to have. You may include a mask which
means that you do not have to test all the bits. You may also use
letters (as in chmod(1)) to specify the mode and mask.

The numeric syntax uses one or two octal numbers of 3 or 4 digits
seperated by a slash. For example, the following makes sure that
only the user (owner of the file) has write permissions and
that all execution permissions are turned off:

    0200/0333

On the other hand, you could forcibly want the file to have this
exact mode:

    0640

The letters are defined with three parts:

    <who><operator><perms>

where:

* `who` -- defines which section of the permissions are being
           checked, the supported sections are:

  * `u` -- the user (owner) of the mode (0700)
  * `g` -- the group of the mode (0070)
  * `o` -- the others of the mode (0007)
  * `a` -- all of them (0777)

* `operator` -- defines how the flags are going to be used;
                these are slight different from the chmod
		command line tool

  * `+` -- the specified flags must be set for that specific user
  * `-` -- the specified flags must not be set for that specific user
  * `=` -- the specified flags must match one to one

* `perms` -- defines a set of permissions that are to be set
             or not; these are:

  * `r` -- the read flag (444)
  * `w` -- the write flag (222)
  * `x` -- the execute flag (111)
  * `s` -- the user or group flag (6000)
  * `t` -- the sticky flag (1000)

The `r`/`w`/`x` flags are ANDed as is with the `ugoa` flags; the 's' flag
is 4000 if `u` or `a` were specified, and 2000 if `g` or `a` were
specified; the `t` is always added if present.

You do not specified a mode mask with letters, instead the operator
defines it for you. When using the:

* `+` -- the mask is set equal to the the mode,
* `-` -- uses the mode as the mask and then set the mode itself to zero,
* `=` -- sets the mask to 07777.

If you need something different, you may have to use the numeric syntax
instead, especially because we do not support multiple entries. So the
example above (0640) can't be represented with the letters scheme.

## Content Check

The content of the log file can also be searched using a regular expression
pattern. Each pattern you define must be defined in a separate section.
The name of the section is used in the reports. It is ignored otherwise.

### Starting a New Section `[<name>]`

You start a new section by writing a name within square brackets.

You can start a new section right after another.

### Regular Expression `regex=<pattern>`

The regular expression used to search each log file that matched the
`pattern=...` parameter.

### Report As Error `report_as=<error>`

Whether to report findings as errors or just warnings. If this parameter
is set to the word "error", then any matches found with the regular
expression will be reported as an error.

