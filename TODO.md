
A few things that still need to be done in this project:

* Implement all the network shit... listener, PING reply, data sharing

**  With the getifaddr() function we get all the information, per
    interface, of the amount of data transferred and received.
    We should look into a way to use that data (knowing that it
    does not get reset...)

    Note: the other method to gather information from the network
          is to use the ioctl() function with the netdevice structure
          (see "man 7 netdevide")

* Plugin for Snap! Websites to look at the data.

* Check each domain name SOA; this test is to verify that the BIND service
  was properly restarted; it can be a plugin inside the `ipmgr` project;
  the idea is to retrieve the SOA of a domain name we manage and then
  compare the serial number against what we have in our files; if wrong,
  then clearly we did not yet properly restart BIND.

  Note: this test has to run only on the BIND service running as the master.
        This plugin should be part of the ipmgr project.

* Move firewall plugin to iplock.

* Look at removing the dependency on procps, just read the /proc/... files
  instead (it is that the procps is a horrible piece of code).
  Also I don't think it's part of newer versions of the OS.

