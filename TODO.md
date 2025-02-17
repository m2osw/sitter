
A few things that still need to be done in this project:

* Look at all references to XML files in our docs (we now use .ini/.conf files).

* Implement all the network functionality... listener, PING reply, data sharing

**  With the getifaddr() function we get all the information, per
    interface, of the amount of data transferred and received.
    We should look into a way to use that data (knowing that it
    does not get reset...)

    Note: the other method to gather information from the network
          is to use the ioctl() function with the netdevice structure
          (see "man 7 netdevide")

* Check the number of ticks between run. If not exactly 1, then our loop is
  too slow and the administrator should be told.

* Do a watchdog of all the services by sending the ALIVE message. Make it
  so we don't swamp the network (i.e. have a separate worker thread which
  sends one message every N seconds--use cppthread paced jobs).

* Replace Tripwire with our own system. This is a plugin using a worker
  thread to check a file's checksum, new files, files that disappeared,
  etc. (very much like tripwire). It will also have to use keys or a
  connection to another computer to get the data checked properly.

  (see cppthread about having a job for a worker thread which will be worked
  on only after a certain date & time--i.e. a paced job)

* Plugin for Snap! Websites to look at the data.

* Check each domain name SOA; this test is to verify that the BIND service
  was properly restarted; it can be a plugin inside the `ipmgr` project;
  the idea is to retrieve the SOA of a domain name we manage and then
  compare the serial number against what we have in our files; if wrong,
  then clearly we did not yet properly restart BIND.

  Note: this test has to run only on the BIND service running as the master.
        This plugin should be part of the ipmgr project.

* Move firewall plugin to iplock.

  Also, it needs fixing since it checks whether ipwall is running to determine
  the firewall status, which is wrong. The functionality is also duplicated
  from the `wait_on_firewall` class found in the iplock library.

* Actually implement real unit tests.

