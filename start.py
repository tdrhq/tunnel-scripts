#!/usr/bin/env python

import os
import subprocess
import signal

# so here's the deal, user gives
class Tunnel:
    ssh_gateway = '61.12.4.27'
    ssh_gateway_user = 'arnold'
    ssh_D_port = 8000 # user doesn't really need to know about this
    tsocks = '/usr/bin/tsocks'
    port = 8100 # this will the local port iptables maps to
    interface = 'wlan0'
    iptables_prefix = ['iptables', '-t', 'nat']
    sudo_user = 'arnold'

    def start_iptables (self):
        # todo: is there another table in the iptables map?
        print ' hello ' + self.ssh_gateway + "\n"
        self.rule = ['-p', 'tcp', '!', '-d', self.ssh_gateway,
                     '-o', self.interface,
                     '-j', 'DNAT', '--to-destination', 
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
        self.tsocks_config_file = os.tempnam ()
        f = open (self.tsocks_config_file, "w")
        f.write ("""
server = 127.0.0.1
server_type = 5
server_port = %s
""" % self.ssh_D_port);
        f.close ()
        print "here\n"
        print self.tsocks_config_file 
        
    def cleanup_tsocks_config (self):
        os.unlink (self.tsocks_config_file)
        self.tsocks_config_file = None

    def run_lcat (self):
        # if your tsocks is not built with ALLOW_ENV_CONFIG, you're in
        # trouble, and I have no way of detecting it!
        lcat_args = ['./lcat', '-p', str(self.port)]
        if (self.sudo_user != None):
            lcat_args = ['sudo', '-u', self.sudo_user] + lcat_args

        os.putenv ('TSOCKS_CONF_FILE', self.tsocks_config_file);
        subprocess.call (["/usr/bin/env"])
        ret = subprocess.call (lcat_args)
        #and we're done!
        print "lcat terminated with %d" % ret
        
    def everything (self):
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


tunnel = Tunnel ()
tunnel.everything ()
