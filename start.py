#!/usr/bin/env python

import os, sys
import signal, subprocess
import getopt, random
import tempfile

class Tunnel:
    ssh_gateway = None
    ssh_gateway_user = None
    ssh_D_port = 0 # user doesn't really need to know about this
    port = 0 # this will the local port iptables maps to
    interface = None
    iptables_prefix = ['iptables', '-t', 'nat']
    sudo_user = None
    
    def parse_opt (self):
        try:
            opts, args = getopt.getopt (sys.argv[1:], "g:l:D:p:i:U:",
                                        ["ssh-gateway", "ssh-user",
                                         "ssh-D-port", "port",
                                         "interface", "sudo-user"]);
        except getopt.GetoptError, err:
            print str(err)
            usage ()
            sys.exit (2)

        for o,a  in opts:
            if o == "-g" or o == "--ssh-gateway":
                self.ssh_gateway = a
            elif o == "-l" or o == "--ssh-user":
                self.ssh_gateway_user = a
            elif o == "-D" or o == "--ssh-D-port":
                self.ssh_D_port = int (a)
            elif o == "-p" or o == "--port":
                self.port = int (a)
            elif o == "-i" or o == "--interface":
                self.interface = a
            elif o == "-U" or o == "--sudo-user":
                self.sudo_user = a

        if (self.ssh_D_port == 0):
            self.ssh_D_port = random.randint (5000, 50000) # arbitrary

        while (self.port == 0 or self.port == self.ssh_D_port):
            self.port = random.randint (5000, 50000)

        if (self.sudo_user == None):
            self.sudo_user = os.getenv ("SUDO_USER")

        
        if (self.ssh_gateway_user == None):
            self.ssh_gateway_user = self.sudo_user

        if (self.ssh_gateway_user == None):
            print "could not guess an SSH login user; please use -l or -U"
            sys.exit (2)

        if (self.ssh_gateway == None):
            print "--ssh-gateway or -g required"
            sys.exit (2)
            
        if (self.interface == None):
            print "Please specify an interface using -i"
            sys.exit (2)

        if (os.getenv ("USER") != "root"): 
            print "Unfortuately this script needs to be run as root. If it is\
any comfort to you, almost every persistent part is run as the sudo user, the root is required only for setting the iptable routes.\n";
            sys.exit (2)

    def usage (self):
        print """
Transparent tunneller using IPTables. In most cases, you better
know what this is doing, if you're using it. :-) However, here's the
simplest form, without any knowledge of what's going on:
   sudo %s -g <gatewaynameorip>

or if you need to use a different account from the SUDO_USER,
   sudo %s -g <gatewaynameorip> -l login

everything else is more or less not required to be changed. But 
here's the synopsis:

TODO!
   
""" % sys.argv [0]
        
    def start_iptables (self):
        # todo: is there another table in the iptables map?
        self.rule = ['-p', 'tcp', '!', '-d', self.ssh_gateway]
        
        if self.interface != None:
            self.rule += ['-o', self.interface]

        self.rule += ['-j', 'DNAT', '--to-destination', 
                     '127.0.0.1:' + str (self.port)]

        subprocess.check_call (
            self.iptables_prefix + 
            ['-A', 'OUTPUT'] +
            self.rule)

    def end_iptables (self):
        subprocess.check_call (
            self.iptables_prefix +
            ['-D', 'OUTPUT'] +
            self.rule)

    def start_ssh_tunnel (self):
        ssh_command = ['ssh', '-N', '-D', str(self.ssh_D_port), '-l', 
                       self.ssh_gateway_user, self.ssh_gateway]

        # do we become another user before doing this? 
        if (self.sudo_user != None):
            ssh_command = ['sudo', '-u', self.sudo_user] + ssh_command

        self.ssh_process = subprocess.Popen (ssh_command)

    def end_ssh_tunnel (self):
        # self.ssh_process.terminate ()
        # self.ssh_process.kill ()
        print "Killing SSH tunnel"
        os.kill (self.ssh_process.pid, signal.SIGKILL)
        self.ssh_process = None

    def restart_ssh_tunnel (self):
        self.end_ssh_tunnel ()
        self.restart_ssh_tunnel ()


    def build_tsocks_config (self):
        handle, self.tsocks_config_file = tempfile.mkstemp ()
        f = open (self.tsocks_config_file, "w")
        f.write ("""
server = 127.0.0.1
server_type = 5
server_port = %s
""" % self.ssh_D_port);
        f.close ()
        
        # sudo_user needs to be able to read this
        os.chmod (self.tsocks_config_file, 0755)
        print "debug: config file: " + self.tsocks_config_file 
        
    def cleanup_tsocks_config (self):
        os.unlink (self.tsocks_config_file)
        self.tsocks_config_file = None

    def run_lcat (self):
        # if your tsocks is not built with ALLOW_ENV_CONFIG, you're in
        # trouble, and I have no way of detecting it!
        lcat_args = ['./lcat', '-p', str(self.port)]
        if (self.sudo_user != None):
            lcat_args = ['sudo', '-E', '-u', self.sudo_user] + lcat_args

        os.putenv ('TSOCKS_CONF_FILE', self.tsocks_config_file);
        ret = subprocess.call (lcat_args)
        #and we're done!
        print "lcat terminated with %d" % ret
        
    def everything (self):
        self.parse_opt ()

        self.start_iptables ()
        self.start_ssh_tunnel ()
        self.build_tsocks_config ()

        try:
            self.run_lcat ()
        except KeyboardInterrupt:
            pass
        
        self.cleanup_tsocks_config ()
        self.end_ssh_tunnel ()
        self.end_iptables ()


os.chdir (os.path.dirname (sys.argv [0]))
tunnel = Tunnel ()
tunnel.everything ()
