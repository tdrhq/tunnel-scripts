#!/usr/bin/env python

import os
import subprocess

# so here's the deal, user gives
class Tunnel:
    ssh_gateway = '61.12.4.27'
    ssh_gateway_user = 'narnold'
    ssh_D_port = 3128 # user doesn't really need to know about this
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
        ssh_command = ['ssh', '-D', str(self.ssh_D_port), '-l', 
                       self.ssh_gateway_user, self.ssh_gateway]

        # do we become another user before doing this? 
        if (self.sudo_user != None):
            ssh_command = ['sudo', '-u', self.sudo_user] + ssh_command

        self.ssh_process = subprocess.Popen (ssh_command)

    def end_ssh_tunnel (self):
        self.ssh_process.terminate ()
        self.ssh_process.kill ()

    def restart_ssh_tunnel (self):
        self.end_ssh_tunnel ()
        self.restart_ssh_tunnel ()

    
tunnel = Tunnel ()
tunnel.start_iptables ()
tunnel.end_iptables ()


